#!/usr/bin/env python

import socket, asyncore, threading, Queue, struct, time, math


def main():
    net = NetworkMaster()
    accel = Accelerometers(net)
    tr = TileRenderer(net, 0x98000)
    m = Map("assets/earthbound_fourside_full.map", width=256)
    ms = MapScroller(tr, m)

    # Cat sprite!
    cat = tr.sprite[0]
    cat.setSize(64, 64)
    cat.setImage(0xd0000)

    x, y = 155, 1689
    speed = 12

    while True:
        x = min(2048-160, max(0, x + accel.x * speed))
        y = min(2048-160, max(0, y + accel.y * speed))
        ms.scroll(x, y)

        cat.moveCenterTo(64 + accel.x*64, 64 + accel.y*64)

        tr.refresh()
        accel.next()


class MapScroller:
    """Implements efficient tear-free scrolling, by only updating
       tiles that are off-screen, just before they come on-screen.
       """

    def __init__(self, renderer, mapobj):
        self.tr = renderer
        self.map = mapobj

    def scroll(self, x, y):
        # Scroll to the specified coordinates. We could be WAY smarter about
        # the actual process of doing this update, but right now I'm implementing
        # it in a totally brute-force manner to keep this test harness simple.
        #
        # This method works because 'blit' knows how to wrap around on the VRAM
        # buffer, and because our TileRenderer is good at removing unnecessary changes.

        x = int(x + 0.5)
        y = int(y + 0.5)
        self.tr.pan(x % 160, y % 160)
        self.tr.blit(self.map, dstx=x>>3, dsty=y>>3,
                     srcx=x>>3, srcy=y>>3, w=17, h=17)


class Map:
    """Container for tile-based map/index data."""

    def __init__(self, filename=None, width=None):
        self.width = width
        if filename:
            self.loadFromFile(filename)

    def loadFromFile(self, filename):
        self.loadFromString(open(filename, "rb").read())

    def loadFromString(self, s):
        self.loadFromArray(struct.unpack("<%dH" % (len(s) / 2), s))

    def loadFromArray(self, data):
        self.data = data
        self.height = len(data) // self.width


class Sprite:
    """Abstraction for one sprite engine. Owned by the TileRenderer."""
    def __init__(self, tr, vramAddr):
        self.tr = tr
        self.vramAddr = vramAddr
        self.x, self.y = 0, 0
        self.setImage(0)
        self.setSize(0, 0)

    def setSize(self, x, y):
        """Set a new size. Must be a power of two."""
        self.size = (x, y)

        if x and y:
            # Configure the mask registers
            self.tr.vram[self.vramAddr + 2] = 0xFF & (0xFF ^ (x - 1))
            self.tr.vram[self.vramAddr + 3] = 0xFF & (0xFF ^ (y - 1))
        else:
            # Sprite disabled
            self.tr.vram[self.vramAddr + 2] = 0xFF
            self.tr.vram[self.vramAddr + 3] = 0xFF

    def setImage(self, flashAddr):
        self.addrH = (flashAddr >> 13) & 0xFE
        self.addrL = (flashAddr >> 6) & 0xFE

        # Update high byte of address
        self.tr.vram[self.vramAddr + 4] = self.addrH

        # Update the low byte
        self.moveTo(self.x, self.y)

    def moveTo(self, x, y):
        """Move the upper left corner to pixel location (x, y)"""

        self.x, self.y = x, y
        x = -int(x)
        y = -int(y)

        # Calculate the X/Y accumulators
        self.tr.vram[self.vramAddr + 0] = 0xFF & x
        self.tr.vram[self.vramAddr + 1] = 0xFF & y

        # Update the low address byte
        # Account for offset when Y is partially clipped
        self.tr.vram[self.vramAddr + 5] = self.addrL + max(0, y) * 2

    def moveCenterTo(self, x, y):
        self.moveTo(x - self.size[0]/2, y - self.size[1]/2)


class TileRenderer:
    """Abstraction for the tile-based graphics renderer. Keeps an in-memory
       copy of the cube's VRAM, and sends updates via the radio as necessary.
       """
 
    # Telemetry packet offset
    FRAME_COUNT = 0

    MAX_FRAMES_AHEAD = 3
    WARP_FRAMES_AHEAD = 10
    
    def __init__(self, net, baseAddr):
        self.net = net
        self.baseAddr = baseAddr
        self.vram = [0] * 1024
        self.dirty = {}
        
        # Sprite renderers
        self.sprite = [Sprite(self, 803 + 6*i) for i in range(2)]

        # Flow control
        self.local_frame_count = 0
        self.remote_frame_count = 0
        self.net.register_callback(self._telemetryCb)

    def _telemetryCb(self, t):
        self.remote_frame_count = ord(t[self.FRAME_COUNT])

    def refresh_alt(self):
        # XXX: This is an alternate refresh implementation that includes
        #      proper flow control and at least some manner of update
        #      optimization... but this python code is so slow that it's
        #      running kinda badly right now.

        # Flow control. We aren't staying strictly synchronous with
        # the cube firmware here, since that would leave the cube idle
        # when it could be rendering. But we'll keep a coarser-grained count of
        # how much faster we are than the cube, and slow down if needbe.

        self.local_frame_count = (self.local_frame_count + 1) & 0xFF
        while True:
            ahead = 0xFF & (self.local_frame_count - self.remote_frame_count)

            if ahead <= self.MAX_FRAMES_AHEAD:
                break
            elif ahead > self.WARP_FRAMES_AHEAD:
                # Way behind! Let's do a time warp...
                self.local_frame_count = self.remote_frame_count
            else:
                # XXX: Ugly way to kill time...
                time.sleep(0)
            
        # Trigger the firmware to refresh the LCD 
        self.vram[802] = self.local_frame_count
        self.dirty[802 // 31] = True

        chunks = self.dirty.keys()
        chunks.sort()
        self.dirty = {}    
        
        for chunk in chunks:
            bytes = [chunk] + self.vram[chunk * 31:(chunk+1) * 31]
            self.net.send_msg(''.join(map(chr, bytes)))

    def refresh(self):
        for chunk in range(0x20):
            bytes = [chunk] + self.vram[chunk * 31:(chunk+1) * 31]
            self.net.send_msg(''.join(map(chr, bytes)))

    def pan(self, x, y):
        """Set the hardware panning registers"""
        self.vram[800] = x
        self.vram[801] = y
        #self.dirty[800 // 31] = True

    def plot(self, tile, x, y):
        """Draw a tile in the hardware tilemap, by coordinate and tile index."""
        addr = self.baseAddr + (tile << 7)
        offset = (x % 20)*2 + (y % 20)*40
        self.vram[offset] = (addr >> 6) & 0xFE
        self.vram[offset+1] = (addr >> 13) & 0xFE
        #self.dirty[offset // 31] = True

    def fill(self, tile, x=0, y=0, w=20, h=20):
        """Fill a box of tiles, all using the same index"""
        for i in range(w):
            for j in range(h):
                self.plot(tile, x+i, y+j)

    def blit(self, mapobj, dstx=0, dsty=0, srcx=0, srcy=0, w=None, h=None):
        """Copy a rectangular block of tile indices from a map to the hardware buffer"""
        w = w or mapobj.width
        h = h or mapobj.height
        for j in xrange(h):
            for i in xrange(w):
                self.plot(mapobj.data[(srcx+i) + mapobj.width * (srcy+j)], dstx+i, dsty+j)


class Accelerometers:
    """Data aggregator for the accelerometer sensors"""
    def __init__(self, net):
        self.net = net
        self.x = self.y = self.xCount = self.yCount = 0
        self.next()
        self.net.register_callback(self._telemetryCb)

    def next(self):
        """Reset the aggregation buffer, and calcuate averages for the last window of time"""
        if self.xCount:
            self.x = self.xTotal / 128.0 / self.xCount - 1.0
        if self.yCount:
            self.y = self.yTotal / 128.0 / self.yCount - 1.0

        self.xCount = self.yCount = 0
        self.xTotal = self.yTotal = 0

    def _telemetryCb(self, data):
        """Aggregate the per-packet telemetry data"""
        (xCount, yCount, xTotal, yTotal) = struct.unpack("<BBHH", data[1:7])
        self.xCount += xCount
        self.xTotal += xTotal
        self.yCount += yCount
        self.yTotal += yTotal


class NetworkMaster(asyncore.dispatcher_with_send):
    """Simple radio interface emulation, using nethub.

       - Incoming telemetry packets simply update the 'latest' telemetry state,
         and optionally invoke any registered callbacks.

       - Outgoing packets are sent as fast as possible. If we have any 'dead air'
         in between packets, we'll send a 'ping' just to get a fresh telemetry
         update in response.

       """    
    LOCAL_TX_DEPTH = 2 		# Depth of local transmit queue
    REMOTE_TX_DEPTH = 2		# Number of unacknowledged transmitted packets in flight

    def __init__(self, address=0x020000e7e7e7e7e7, host="127.0.0.1", port=2405):
        asyncore.dispatcher_with_send.__init__(self, map={})

        self.recv_buffer = ""
        self.tx_queue = Queue.Queue(self.LOCAL_TX_DEPTH)
        self.tx_depth = 0
        self.ping_buffer = struct.pack("<BBQB", 9, 1, address, 0x20)
        
        self.callbacks = []
        self.telemetry = [0] * 32
        self.address = address

        self.create_socket(socket.AF_INET, socket.SOCK_STREAM)
        self.connect((host, port))
        self.socket.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)

        self.thread = threading.Thread(target=self._threadMain)
        self.thread.daemon = True
        self.thread.start()

    def _threadMain(self):
        self._pump_tx_queue()
        asyncore.loop(map=self._map)

    def _pump_tx_queue(self):
        if self.tx_depth < self.REMOTE_TX_DEPTH:
            self.tx_depth += 1
            try:
                self.send(self.tx_queue.get(block=False))
            except Queue.Empty:
                self.send(self.ping_buffer)

    def register_callback(self, cb):
        self.callbacks.append(cb)

    def send_msg(self, payload):
        data = struct.pack("<BBQ", 8+len(payload), 1, self.address) + payload
        while True:
            try:
                self.tx_queue.put(data, timeout=1)
                return
            except Queue.Full:
                # Try again (Don't block for too long, or the process will be hard to kill)
                pass

    def handle_read(self):
        self.recv_buffer += self.recv(8192)
        while self.recv_buffer:
            packetLen = ord(self.recv_buffer[0]) + 2
            if len(self.recv_buffer) >= packetLen:
                # We can handle one message
                self._handle_packet(self.recv_buffer[:packetLen])
                self.recv_buffer = self.recv_buffer[packetLen:]
            else:
                # Waiting
            	return

    def _handle_packet(self, packet):
        msg_type = packet[1]

        if msg_type == '\x01':
            # ACK
            self.send("\x00\x02")

            # Store telemetry updates, and immediately notify callbacks
            if len(packet) > 10:
                self.telemetry = packet[10:]
                for cb in self.callbacks:
                    cb(self.telemetry)

        elif msg_type == '\x02':
            # ACK
            self.tx_depth -= 1
            self._pump_tx_queue()

        elif msg_type == '\x03':
            # NAK. Handle this like an ACK too, but delay
            # before sending anything else, so we don't spin
            # eating CPU if there's no destination to rate-limit
            # our transmissions.
            time.sleep(0.1)
            self.tx_depth -= 1
            self._pump_tx_queue()


if __name__ == "__main__":
    main()
