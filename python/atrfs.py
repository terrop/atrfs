#!/usr/bin/env python
# -*- coding: utf-8 -*-
# atrfs.py - 1.5.2009 - 2.5.2009 Ari & Tero Roponen

import errno, fuse, os, stat, xattr
import timing

import sys
sys.path.append(".")
from FLVTree import VirtualFile, FLVFile, FLVDirectory
from asc import *

def flv_categorize(parent, name):
	entry = parent.lookup(name)
	if not isinstance(entry, FLVFile):
		return
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

def pid_to_command(pid):
	try:
		f = file("/proc/%d/cmdline" % pid)
		return f.readline().split("\x00")[0].split("/")[-1]
	finally:
		f.close()

def add_asc_file(flv_path):
	entry = flv_resolve_path(flv_path)
	real_flv = entry.get_real_name()
	text = asc_read_subtitles("%s.asc" % real_flv[:-4], "it")
	name = "%s.srt" % flv_path.split(os.path.sep)[-1][:-4]
	asc_ent = VirtualFile(text)
	parent = flv_resolve_path(flv_path[:flv_path.rindex(os.path.sep)])
	parent.add_entry(name, asc_ent)
	return (parent, name)

def del_asc_file(data):
	parent, name = data
	parent.del_entry(name)

class FLVFuseFile():
	def __init__(self, fuse, path, flags, *mode):
		self.entry = flv_resolve_path(path)
		self.cmd = None
		self.asc = None
		if isinstance(self.entry, FLVFile):
			self.cmd = pid_to_command(fuse.GetContext()["pid"])
			self.file = file(self.entry.get_real_name())
			if self.cmd in ["mplayer"]:
				self.asc = add_asc_file(path)
				timing.start()
		elif isinstance(self.entry, VirtualFile):
			pass
		else:
			raise IOError("Unknown file type")

	def read(self, size, offset):
		if isinstance(self.entry, FLVFile):
			self.file.seek(offset)
			return self.file.read(size)
		elif isinstance(self.entry, VirtualFile):
			return self.entry.get_contents()[offset:offset+size]
		else:
			raise IOError("Unknown file type")

	def release(self, flags):
		if isinstance(self.entry, FLVFile):
			self.file.close()
			if self.cmd in ["mplayer"]:
				timing.finish()
				last_time = flv_resolve_path("/stat/last-time")
				if last_time:
					last_time.set_contents("%s\n" % timing.milli())
				if self.asc:
					del_asc_file(self.asc)
		else:
			pass

class ATRFS(fuse.Fuse):
	def __init__(self):
		# Hack to allow FLVFuseFile to see this class
		class wrapped_file_class(FLVFuseFile):
			def __init__(self2, *a, **kw):
				FLVFuseFile.__init__(self2, self, *a, **kw)
		self.file_class = wrapped_file_class
		fuse.Fuse.__init__(self)

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
		elif isinstance(entry, VirtualFile):
			st.st_mode = stat.S_IFREG | 0444
			st.st_size = len(entry.get_contents())
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
	# stat-directory
	stat = FLVDirectory("stat")
	flv_root.add_entry("stat", stat)
	ver = VirtualFile("ATRFS 1.0 (python version)\n")
	stat.add_entry("version", ver)
	time = VirtualFile("(empty)\n")
	stat.add_entry("last-time", time)

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
