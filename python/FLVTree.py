# FLVTree.py - 2.5.2009 - 6.5.2009 Ari & Tero Roponen -*- coding: utf-8 -*-
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
	def __init__(self, contents="", update_fun = None):
		BaseFile.__init__(self)
		self.data = contents
		self.updater = update_fun
	def __repr__(self):
		return "<VirtualFile (%d bytes)>" % len(self.data)
	def get_contents(self):
		if self.updater:
			self.updater(self)
		return self.data
	def set_contents(self, contents):
		self.data = contents

class FLVFile(BaseFile):
	flv_dirs = []

	def __init__(self, directory, file_name):
		BaseFile.__init__(self)
		if directory not in self.flv_dirs: self.flv_dirs.append(directory)
		self.real_dir_idx = self.flv_dirs.index(directory)
		self.real_name = file_name

	def __repr__(self):
		return "<FLVFile '%s'>" % self.real_name

	def get_real_name(self):
		return os.path.join(self.flv_dirs[self.real_dir_idx], self.real_name)

	def _get_flv_len(self, name):
		cmd = "mplayer -identify -frames 0 -ao null -vo null 2>/dev/null -- \"%s\"" % name
		f = os.popen(cmd)
		lines = f.readlines()
		f.close()
		return filter(lambda(line):line[:10]=="ID_LENGTH=", lines)[0][10:-1]

	def _get_attr_str(self, attr, default):
		name = self.get_real_name()
		if attr in xattr.list(name):
			str = xattr.get(name, attr)
		else:
			str = "%s\x00" % default
		return str[:-1]

	def _set_attr_str(self, attr, value):
		name = self.get_real_name()
		str = "%s\x00" % value
		xattr.setxattr(name, attr, str)

	def get_count(self):
		return int(self._get_attr_str("user.count", "0"))
	def set_count(self, count):
		self._set_attr_str("user.count", "%d" % count)
	def get_watchtime(self):
		return int(float(self._get_attr_str("user.watchtime", "0")))
	def set_watchtime(self, time):
		self._set_attr_str("user.watchtime", "%2.2f" % time)
	def get_length(self):
		name = self.get_real_name()
		if not "user.length" in xattr.list(name):
			lenstr = self._get_flv_len(name)
			self._set_attr_str("user.length", lenstr)
		return int(float(self._get_attr_str("user.length", "600")))

class FLVDirectory(BaseFile):
	def __init__(self, name):
		BaseFile.__init__(self)
		self.name = name
		self.contents = {}

	def __repr__(self):
		return "<FLVDir %s (%d entries)> " % (self.name, len(self.contents))

	def add_entry(self, name, entry, uniquify = False):
		tmp = name
		count = 0
		while tmp in self.contents and uniquify:
			count = count + 1
			tmp = "%s_%d.flv" % (name[:-4], count)
		if tmp in self.contents:
			raise IOError("Name already in use: %s" % tmp)
		self.contents[tmp] = entry
		entry.set_pos(self, tmp)

	def del_entry(self, name):
		entry = self.contents[name]
		entry.set_pos(None, None)
		del(self.contents[name])
		if (len(self.contents) == 0):
			(parent, name) = self.get_pos()
			if (name != "/"):
				parent.del_entry(name)

	def lookup(self, name):
		return self.contents.get(name,  None)

	def get_names(self):
		return self.contents.keys()
