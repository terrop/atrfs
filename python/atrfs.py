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
	with open("/proc/%d/cmdline" % pid) as f:
		return f.readline().split("\x00")[0].split("/")[-1]

def add_asc_file(flv_entry):
	parent, flv_name = flv_entry.get_pos()
	asc_name = "%s.srt" % flv_name[:-4]
	real_flv = flv_entry.get_real_name()
	real_asc = "%s.asc" % real_flv[:-4]
	text = None
	for lang in flv_languages.split(","):
		if not text:
			try:
				text = asc_read_subtitles(real_asc, lang)
			except IOError, e:
				pass
	if not text:
		text = asc_fake_subtitles(flv_name[:-4], 600) # TODO: oikea pituus
	asc_entry = VirtualFile(text)
	parent.add_entry(asc_name, asc_entry)
	return asc_entry

def del_asc_file(asc_entry):
	parent, name = asc_entry.get_pos()
	parent.del_entry(name)

class FLVFuseFile():
	def __init__(self, fuse, path, flags, *mode):
		self.entry = flv_resolve_path(path)
		if isinstance(self.entry, FLVFile):
			self.cmd = pid_to_command(fuse.GetContext()["pid"])
			self.file = file(self.entry.get_real_name())
			if self.cmd in ["mplayer"]:
				self.asc = add_asc_file(self.entry)
				timing.start()
			else:
				self.asc = None
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
		global flv_recent
		if isinstance(self.entry, FLVFile):
			self.file.close()
			if self.cmd in ["mplayer"]:
				timing.finish()
				last_time = flv_resolve_path("/stat/last-time")
				if last_time:
					last_time.set_contents("%s\n" % timing.milli())
				if self.asc:
					del_asc_file(self.asc)
				# update recent list
				parent, name = self.entry.get_pos()
				flv_recent.insert(0, name)
				flv_recent = flv_recent[:10] # max 10 items
				recent = flv_resolve_path("/stat/recent")
				recent.set_contents("\n".join(flv_recent) + "\n")
		else:
			pass

	def write(self, buf, offset):
		global flv_languages
		if not self.entry == flv_resolve_path("/stat/language"):
			return -errno.EROFS
		# Check that the string is valid: "xx,yy,zz"
		for elt in buf.rstrip().split(","):
			if len(elt) != 2:
				return -errno.ERANGE
		flv_languages = buf
		self.entry.set_contents(flv_languages)
		return len(buf)

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
			st.st_mode = stat.S_IFREG | 0666
			st.st_size = len(entry.get_contents())
			st.st_nlink = 1
		return st

	def readdir(self, path, offset):
		entry = flv_resolve_path(path)
		for name in entry.get_names():
			yield fuse.Direntry(name)

	def truncate(self, path, len):
		return 0

def flv_populate(dir):
	for directory, subdirs, filenames in os.walk(dir):
		for name in filter(lambda (name): name[-4:] == ".flv", filenames):
			flv_root.add_entry(name, FLVFile(directory, name), True)
	for name in flv_root.get_names():
		flv_categorize(flv_root, name)
	# stat-directory
	stat = FLVDirectory("stat")
	flv_root.add_entry("stat", stat)
	# version
	ver = VirtualFile("ATRFS 1.0 (python version)\n")
	stat.add_entry("version", ver)
	# last-time
	time = VirtualFile("(empty)\n")
	stat.add_entry("last-time", time)
	# language
	lang = VirtualFile(flv_languages)
	stat.add_entry("language", lang)
	# recent
	recent = VirtualFile("(empty)")
	stat.add_entry("recent", recent)

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
flv_languages = "fi,en,la,it"
flv_recent = []
def main():
	global flv_root
	flv_root = FLVDirectory("/")
	flv_parse_config_file("atrfs.conf")
	server = ATRFS()
	server.parse()
	server.main()

main()
