#!/usr/bin/env python
# -*- coding: utf-8 -*-
# atrfs.py - 1.5.2009 - 6.5.2009 Ari & Tero Roponen

import errno, fuse, os, stat, xattr
import timing

import sys
sys.path.append(".")
from FLVTree import VirtualFile, FLVFile, FLVDirectory
from asc import *

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
	length = flv_entry.get_length()
	val = 1.0 * flv_entry.get_watchtime() / length
	timestr = "%02d:%02d" % (length / 60, length % 60)
	asc_name = "%s.srt" % flv_name[:-4]
	real_flv = flv_entry.get_real_name()
	real_asc = "%s.asc" % real_flv[:-4]
	text = None
	for lang in stats.get_lang_entry().get_contents().split(","):
		if not text:
			try:
				text = asc_read_subtitles(real_asc, lang, True)
			except IOError, e:
				pass
	if text:
		text.insert(2 + text.index("0\n"), "%2.2f × %s\n" % (val, timestr))
		text = "".join(text)
	else:
		title = "%s\n%2.2f × %s" % (flv_name[:-4], val, timestr)
		text = asc_fake_subtitles(title, length)
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
		if isinstance(self.entry, FLVFile):
			self.file.close()
			if self.cmd in ["mplayer"]:
				timing.finish()
				# watchtime is in seconds
				wt = (timing.milli() / 1000.0) + self.entry.get_watchtime()
				self.entry.set_watchtime(wt)
				self.entry.set_count(self.entry.get_count() + 1)
				stats.categorize(self.entry)
				stats.update_recent(self.entry)
				stats.update_toplist(self.entry)
				stats.update_lastlist(self.entry)
				if self.asc:
					del_asc_file(self.asc)
		else:
			pass

	def write(self, buf, offset):
		lang = stats.get_lang_entry()
		if not self.entry == lang:
			return -errno.EROFS
		# Check that the string is valid: "xx,yy,zz"
		for elt in buf.rstrip().split(","):
			if len(elt) != 2:
				return -errno.ERANGE
		lang.set_contents(buf.rstrip())
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

class FLVStatistics():
	def __init__(self, entries):
		self.root = FLVDirectory("stat")
		self.lang = VirtualFile("fi,en,la,it")
		self.root.add_entry("language", self.lang)
		self.top = VirtualFile("", self._update_top_file)
		self.root.add_entry("top-list", self.top)
		self.last = VirtualFile("", self._update_last_file)
		self.root.add_entry("last-list", self.last)
		self.recent = VirtualFile("", self._update_recent_file)
		self.root.add_entry("recent", self.recent)

		for ent in entries:
			self.categorize(ent)
		entries = sorted(entries, self._cmp_for_top)

		self.rlist = []
		self.toplist = entries[:10]
		self.lastlist = entries[-10:]

	# These methods update the file contents just before they are used.
	def _update_recent_file(self, rfile):
		rfile.set_contents("".join(map(self._entry_to_recent_str, self.rlist)))
	def _update_top_file(self, tfile):
		tfile.set_contents("".join(map(self._entry_to_timed_str, self.toplist)))
	def _update_last_file(self, lfile):
		lfile.set_contents("".join(map(self._entry_to_timed_str, self.lastlist)))

	def get_root(self):
		return self.root
	def get_lang_entry(self):
		return self.lang
	def _entry_to_recent_str(self, entry):
		(parent, name) = entry.get_pos()
		(_, pname) = parent.get_pos()
		return "%s%c%s\n" % (pname, os.path.sep, name)
	def _entry_to_timed_str(self, entry):
		time = entry.get_watchtime()
		(parent, name) = entry.get_pos()
		(_, pname) = parent.get_pos()
		return "%02d:%02d\t%s%c%s\n" % (time / 60, time % 60, pname, os.path.sep, name)
	def _cmp_for_top(self,ent1, ent2):
		c1 = ent1.get_watchtime()
		c2 = ent2.get_watchtime()
		return c2 - c1
	def _cmp_for_last(self,ent1, ent2):
		return self._cmp_for_top(ent2, ent1)

	# These methods update the lists that are used to create stat-files' contents.
	def update_recent(self, entry):
		if not entry in self.rlist[:1]:
			self.rlist.insert(0, entry)
			self.rlist = self.rlist[:10] # max 10 items
	def update_toplist(self, entry):
		if not entry in self.toplist:
			self.toplist.insert(0, entry)
		self.toplist = sorted(self.toplist, self._cmp_for_top)
		self.toplist = self.toplist[:10]
	def update_lastlist(self, entry):
		if not entry in self.lastlist:
			self.lastlist.insert(0, entry)
		self.lastlist = sorted(self.lastlist, self._cmp_for_last)
		self.lastlist = self.lastlist[:10]

	def get_category_name(self, entry):
		# cat = (100 * average watchtime) / len
		count = entry.get_count()
		if count == 0:
			avg = 0
		else:
			avg = entry.get_watchtime() / count
		return str((100 * avg) / entry.get_length())
	def categorize(self, entry):
		if not isinstance(entry, FLVFile):
			return
		cat = self.get_category_name(entry)
		cat_dir = flv_root.lookup(cat)
		if not cat_dir:
			cat_dir = FLVDirectory(cat)
			flv_root.add_entry(cat, cat_dir)
		(parent, name) = entry.get_pos()
		parent.del_entry(name)
		cat_dir.add_entry(name, entry)

def flv_parse_config_file(filename):
	global def_lang, all_entries
	cfg = file(filename)
	lines = cfg.readlines()
	cfg.close()
	for line in lines:
		if line[0] == "#": pass
		elif line[:9] == "language=":
			def_lang = line[9:].rstrip()
		else:		# add files in directory (line ends with '\n'
			for d, sd, fnames in os.walk(line[:-1]):
				for name in filter(lambda (n): n[-4:] == ".flv", fnames):
					ent = FLVFile(d, name)
					flv_root.add_entry(name, ent, True)
					all_entries.insert(0, ent)

fuse.fuse_python_api = (0,2)
flv_root = None
stats = None
def_lang = None
all_entries = None
def main():
	global flv_root, stats, def_lang, all_entries
	flv_root = FLVDirectory("/")
	def_lang = "fi,en,la,it"
	all_entries = []
	flv_parse_config_file("atrfs.conf")

	stats = FLVStatistics(all_entries)
	flv_root.add_entry("stat", stats.get_root())
	stats.get_lang_entry().set_contents(def_lang)
	all_entries = None

	server = ATRFS()
	server.parse()
	server.main()

main()
