# reads the tmx file format

import lxml.etree
import os.path

class Map:
	def __init__(self, path):
		doc = lxml.etree.parse(path)
		self.name = path[0:-4]
		self.width = int(doc.getroot().get("width"))
		self.height = int(doc.getroot().get("height"))
		self.pw = 16 * self.width
		self.ph = 16 * self.height
		self.tilesets = [TileSet(self, elem) for elem in doc.findall("tileset")]
		self.layers = [Layer(self, elem) for elem in doc.findall("layer")]
		self.layer_dict = dict((layer.name,layer) for layer in self.layers)
		self.objects = [Obj(self, elem) for elem in doc.findall("objectgroup/object")]
		self.object_dict = dict((obj.name, obj) for obj in self.objects)

	def gettile(self, gid):
		for result in (tileset.gettile(gid) for tileset in self.tilesets):
			if result is not None:
				return result
		return None

class TileSet:
	def __init__(self, map, xml):
		self.map = map
		self.name = xml.get("name")
		self.gid = int(xml.get("firstgid"))
		img = xml.find("image")
		self.imgpath = img.get("source")
		if not os.path.exists(self.imgpath):
			raise Exception("TileSet image missing:  %s" % self.imgpath)
		self.pw = int(img.get("width"))
		self.ph = int(img.get("height"))
		self.width = self.pw/16
		self.height = self.ph/16
		self.count = self.width * self.height
		self.tiles = [Tile(self, lid) for lid in range(self.count)]
		for node in xml.findall("tile/properties"):
			self.tiles[int(node.getparent().get("id"))].props = \
				dict((prop.get("name").lower(), prop.get("value")) for prop in node)

	def tileat(self, x, y):
		return self.tiles[x + y * self.width]

	def gettile(self, gid):
		lid = gid - self.gid
		if lid >= 0 and lid < self.count:
			return self.tiles[lid]
		return None

class Tile:
	def __init__(self, tileset, lid):
		self.tileset = tileset
		self.lid = lid
		self.gid= tileset.gid + self.lid
		self.x = lid % tileset.width
		self.y = lid / tileset.width
		self.props = {}

class Layer:
	def __init__(self, map, xml):
		self.map = map
		self.name = xml.get("name")
		self.width = int(xml.get("width"))
		self.height = int(xml.get("height"))
		self.visible = xml.get("visible", "0") != "0"
		self.opacity = float(xml.get("opacity", "1"))
		self.tiles = [int(ch) for l in xml.findtext("data").strip().splitlines() for ch in l.split(',') if len(ch) > 0]
		self.props = dict((prop.get("name").lower(), prop.get("value")) for prop in xml.findall("properties/property"))
		
	def tileat(self, x, y):
		return self.map.gettile(self.tiles[x + y * self.width])

	def gettileset(self):
		for tile in (self.map.gettile(t) for t in self.tiles):
			if tile is not None:
				return tile.tileset
		return None

class Obj:
	def __init__(self, map, xml):
		self.map = map
		self.name = xml.get("name")
		self.type = xml.get("type")
		self.px = xml.get("x")
		self.py = xml.get("y")
		self.pw = xml.get("width")
		self.ph = xml.get("height")
		self.tile = map.gettile(int(xml.get("gid", "0")))
		self.props = dict((prop.get("name").lower(), prop.get("value")) for prop in xml.findall("properties/property"))
