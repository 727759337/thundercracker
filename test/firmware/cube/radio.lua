--[[
    Radio utilities for the Sifteo Thundercracker simulator's scripting environment

    M. Elizabeth Scott <beth@sifteo.com> 
    Copyright <c> 2011 Sifteo, Inc. All rights reserved.
]]--

-- General Utilities

local bit = require("bit")

function packHex(hexStr)
    -- Hex to string
    local r = {}
    for hexByte in hexStr:gmatch("..") do
        r[1+#r] = string.char(tonumber(hexByte, 16))
    end
    return table.concat(r)
end

function packHexN(hexStr)
    -- Hex to string, with nybble swapping
    local r = {}
    for hexByte in hexStr:gmatch("..") do
        local num = tonumber(hexByte, 16)
        num = bit.bor(bit.lshift(bit.band(num, 0x0F), 4),
                      bit.rshift(bit.band(num, 0xF0), 4))
        r[1+#r] = string.char(num)
    end
    return table.concat(r)
end

function hexDump(buf)
    -- http://lua-users.org/wiki/HexDump
    for i=1,math.ceil(#buf/16) * 16 do
        if (i-1) % 16 == 0 then io.write(string.format('%08X  ', i-1)) end
        io.write( i > #buf and '   ' or string.format('%02X ', buf:byte(i)) )
        if i %  8 == 0 then io.write(' ') end
        if i % 16 == 0 then io.write( buf:sub(i-16+1, i):gsub('%c','.'), '\n' ) end
    end
end

function unpackHex(buf)
    -- Inverse of packHex()
    local r = {}
    for i = 1, #buf do
        r[i] = string.format("%02X", buf:byte(i))
    end
    return table.concat(r)
end


-- Radio Utilities

radio = {}

    function radio:tx(hexString)
        -- Transmit a hex packet, wait for it to be processed.

        local ack = gx.cube:handleRadioPacket(packHex(hexString))
        gx.sys:vsleep(0.01)
        return ack
    end

    function radio:txn(hexString)
        -- Nybble-swapped version of tx(). Our overall protocol is
        -- nybble-based, so unless we're sending flash escape data it usually
        -- makes more sense to handle each nybble as little-endian rather than
        -- left-to-right (big endian).
       
        local ack = gx.cube:handleRadioPacket(packHexN(hexString))
        gx.sys:vsleep(0.01)
        return ack
    end

    function radio:expectQuery(idByte)
        -- Expect a query result to arrive. Poll for that, for a limited
        -- time, while ignoring any normal ACK packets that come back.

        local deadline = gx.sys:vclock() + 1
        repeat
            packet = self:tx('ff')
            if gx.sys:vclock() > deadline then
                error("Timeout while waiting for query response packet")
            end
        until packet:byte(1) == bit.bor(idByte, 0x80)
        return packet:sub(2)
    end
