#!/usr/bin/env python
# -*- coding: utf-8 -*-
# atrfs.py - 1.5.2009 - 1.5.2009 Ari & Tero Roponen

import errno, fuse, os, stat, xattr

class FLVFile():
	flv_dirs = []

	def __init__(self, directory, file_name):
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

class FLVDirectory():
	def __init__(self, name):
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
			raise IOError("Name already in use")
		self.contents[tmp] = entry

	def del_entry(self, name):
		del(self.contents[name])

	def lookup(self, name):
		return self.contents.get(name,  None)

	def get_names(self):
		return self.contents.keys()

def flv_categorize(parent, name):
	entry = parent.lookup(name)
	category = entry.get_attrs().get("user.count", "0\x00")[:-1]
	cat = flv_root.lookup(category)
	if not cat:
		cat = FLVDirectory(category)
		flv_root.add_entry(category, cat)
	parent.del_entry(name)
	cat.add_entry(name, entry)

def flv_resolve_path(path):
	parts = filter(lambda (str): not str == "", path.split(os.path.sep))
	entry = flv_root
	for part in parts:
		entry = entry.lookup(part)
	return entry

class ATRStat(fuse.Stat):
	def __init__(self):
		self.st_mode = 0
		self.st_ino = 0
		self.st_dev = 0
		self.st_nlink = 0
		self.st_uid = 0
		self.st_gid = 0
		self.st_size = 0
		self.st_atime = 0
		self.st_mtime = 0
		self.st_ctime = 0

class FLVFuseFile():
	def __init__(self, path, flags, *mode):
		self.entry = flv_resolve_path(path)

	def read(self, size, offset):
		f = file(self.entry.get_real_name())
		f.seek(offset)
		buf = f.read(size)
		f.close()
		return buf

class ATRFS(fuse.Fuse):
	file_class = FLVFuseFile
	def getattr(self, path):
		st = ATRStat()
		entry = flv_resolve_path(path)
		if not entry:
			return -errno.ENOENT
		elif isinstance(entry, FLVDirectory):
			st.st_mode = stat.S_IFDIR | 0755
			st.st_nlink = 2
		elif isinstance(entry, FLVFile):
			stbuf = os.lstat(entry.get_real_name())
			# TODO: lisää attribuutteja
			st.st_mode = stat.S_IFREG | 0444
			st.st_size = stbuf.st_size
			st.st_mtime = stbuf.st_mtime
			st.st_nlink = 1
		return st

	def readdir(self, path, offset):
		entry = flv_resolve_path(path)
		for name in entry.get_names():
			yield fuse.Direntry(name)
	
def flv_populate(dir):
	for directory, subdirs, filenames in os.walk(dir):
		for name in filter(lambda (name): name[-4:] == ".flv", filenames):
			flv_root.add_entry(name, FLVFile(directory, name), True)
	for name in flv_root.get_names():
		flv_categorize(flv_root, name)

def flv_parse_config_file(filename):
	cfg = file(filename)
	lines = cfg.readlines()
	cfg.close()
	for line in lines:
		if line[0] == "#": pass
		elif line[:9] == "language=": raise IOError("Not implemented: language")
		else: flv_populate(line[:-1]) # line ends with '\n'

fuse.fuse_python_api = (0,2)
flv_root = None
def main():
	global flv_root
	flv_root = FLVDirectory("/")
	flv_parse_config_file("atrfs.conf")
	server = ATRFS()
	server.parse()
	server.main()

main()
