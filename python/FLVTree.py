# FLVTree.py - 2.5.2009 - 2.5.2009 Ari & Tero Roponen -*- coding: utf-8 -*-
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
		self.data = contents
	def __repr__(self):
		return "<VirtualFile (%d bytes)>" % len(self.data)
	def get_contents(self):
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

	def get_attrs(self):
		attrs = {}
		name = self.get_real_name()
		for attr in xattr.list(name):
			attrs[attr] = xattr.get(name, attr)
		return attrs

	def set_attrs(self, attrs):
		old_attrs = self.get_attrs()
		new_attrs = [ attr for attr, value in attrs.items() if old_attrs[attr] != value ]
		for attr in new_attrs:
			if attrs[attr][-1] != "\x00":
				raise IOError("Attribute value must end with '\x00'")
			print("Change %s from %s to %s" % (attr, old_attrs[attr], attrs[attr]))

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

	def lookup(self, name):
		return self.contents.get(name,  None)

	def get_names(self):
		return self.contents.keys()