# FLVTree.py - 2.5.2009 - 19.12.2009 Ari & Tero Roponen -*- coding: utf-8 -*-
from hashlib import sha1
import sqlite3
import os
import xattr

class BaseFile():
	def __init__(self):
		self.entry_pos = None

	def set_pos(self, parent, name):
		self.entry_pos = (parent, name)

	def get_pos(self):
		return self.entry_pos

class VirtualFile(BaseFile):
	def __init__(self, contents=""):
		BaseFile.__init__(self)
		self.set_contents(contents)

	def __repr__(self):
		return "<VirtualFile (%d bytes)>" % len(self.__data)

	# This method is overridden by subclasses.
	def update_contents(self):
		pass

	def get_contents(self):
		self.update_contents()
		return self.__data

	def set_contents(self, contents):
		self.__data = contents

class DirStatFile(VirtualFile):
	def timestr(self, time):
		return "%02d:%02d:%02d" % (time / 3600, (time % 3600) / 60, time % 60)

	def update_contents(self):
		(parent, _) = self.get_pos()
		time = 0
		for name in parent.get_names():
			ind = name.rfind(".")
			if ind < 0 or name[ind:] not in [".flv", ".webm"]:
				continue
			ent = parent.lookup(name)
			time = time + ent.get_length()
		self.set_contents("Total time: %s\n" % self.timestr(time))


class FLVDatabase():
	def __init__(self, filename):
		self.filename = filename

	def get_attr(self, sha1, attr, default=None):
		conn = sqlite3.connect(self.filename)
		curs = conn.cursor()
		curs.execute("SELECT %s FROM Files WHERE sha1 = \"%s\"" % (attr, sha1))
		for line in curs:
			curs.close()
			conn.close()
			return unicode(line[0])
		curs.close()
		conn.close()
		return default

	def set_attr(self, sha1, attr, value):
		conn = sqlite3.connect(self.filename)
		curs = conn.cursor()
		curs.execute("UPDATE Files SET %s = \"%s\" WHERE sha1 = \"%s\"" % (attr, value, sha1))
		conn.commit()
		curs.close()
		conn.close()

class FLVFile(BaseFile):
	flv_dirs = []
	db = None

	def __init__(self, name):
		BaseFile.__init__(self)
		self.name = name

	def __repr__(self):
		return "<FLVFile '%s'>" % self.name

	def get_real_name(self):
		return FLVFile.files.real_name(self.name)

	def get_attr(self, attr, default=None):
		return FLVFile.db.get_attr(self.get_sha1(), attr, default)

	def set_attr(self, attr, value):
		FLVFile.db.set_attr(self.get_sha1(), attr, value)

	def get_count(self):
		return int(self.get_attr("count", "0"))

	def set_count(self, count):
		self.set_attr("count", "%d" % count)

	def get_watchtime(self):
		return int(float(self.get_attr("watchtime", "0")))

	def set_watchtime(self, time):
		self.set_attr("watchtime", "%2.2f" % time)

	def get_length(self):
		lenstr = self.get_attr("length")
		if not lenstr or lenstr == "0.0":
			name = self.get_real_name()
			cmd = "mplayer -identify -frames 0 -ao null -vo null 2>/dev/null -- \"%s\"" % name
			f = os.popen(cmd)
			lines = f.readlines()
			f.close()
			lenstr = filter(lambda(line):line[:10]=="ID_LENGTH=", lines)[0][10:-1]
			self.set_attr("length", lenstr)
		return int(float(lenstr))

	# sha1 is always put into xattr
	def get_sha1(self):
		name = self.get_real_name()
		if "user.sha1" in xattr.list(name):
			val = xattr.get(name, "user.sha1")[:-1]
		else:
			with open(name) as f:
				val = sha1(f.read()).hexdigest()
			xattr.setxattr(name, "user.sha1", "%s\x00" % val)
		return val

class FLVDirectory(BaseFile):
	def __init__(self, name):
		BaseFile.__init__(self)
		self.name = name
		self.contents = {}
		self.add_entry("stat.txt", DirStatFile())

	def __repr__(self):
		return "<FLVDir %s (%d entries)> " % (self.name, len(self.contents))

	def add_entry(self, name, entry):
		self.contents[name] = entry
		if isinstance(entry, str):
			# FLVFile is stored as a string in order to save memory.
			self.contents[name] = name
		else:
			self.contents[name] = entry
			entry.set_pos(self, name)

	def del_entry(self, name):
		entry = self.lookup(name)
		entry.set_pos(None, None)
		del(self.contents[name])
		if (len(self.contents) == 0):
			(parent, name) = self.get_pos()
			if (name != "/"):
				parent.del_entry(name)

	def lookup(self, name):
		entry = self.contents.get(name, None)
		if isinstance(entry, str):
			entry = FLVFile(name)
			entry.set_pos(self, name)
		return entry

	def get_names(self):
		return self.contents.keys()
