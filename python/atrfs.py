#!/usr/bin/env python
# -*- coding: utf-8 -*-
# atrfs.py - 1.5.2009 - 18.12.2009 Ari & Tero Roponen

import errno, fuse, os, stat
import timing

from FLVTree import VirtualFile, FLVFile, FLVDirectory, FLVDatabase
from asc import *
from files import Files

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
	singer = flv_entry.get_attr("user.singer", None)
	timestr = "%02d:%02d" % (length / 60, length % 60)
	asc_name = "%s.srt" % flv_name[:-4]
	real_flv = flv_entry.get_real_name()
	real_asc = "%s.asc" % real_flv[:-4]
	text = None
	for lang in stats.lookup("language").get_contents().split(","):
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
		text = asc_fake_subtitles(title, length, singer)
	asc_entry = VirtualFile(text)
	parent.add_entry(asc_name, asc_entry)
	return asc_entry

def del_asc_file(asc_entry):
	parent, name = asc_entry.get_pos()
	parent.del_entry(name)

def filter_entry(entry, filt, gvars = {}):
	"Return category if ENTRY passes FILT, else None."
	for var in filt.co_names:
		if var == "sha1":
			gvars[var] = entry.get_sha1()
		elif var == "name":
			gvars[var] = entry.name
		elif var == "count":
			gvars[var] = entry.get_count()
		elif var == "length":
			gvars[var] = entry.get_length()
		elif var == "watchtime":
			gvars[var] = entry.get_watchtime()
		elif var == "cat":
			pass
		elif var == "catfile":
			fname = entry.get_real_name()
			idx = fname.rindex(os.sep)
			catfile = "%s%s" % (fname[:idx + 1], "cat.txt")
			if os.access(catfile, os.R_OK):
				with file(catfile) as f:
					gvars[var] = f.readline().split("\n")[0]
			else:
				gvars[var] = None
		else:
			# Other variables are strings.
			val = entry.get_attr("user.%s" % var, "")
			gvars[var] = val
	gvars["str"] = str	# This must be available in the environment.
	gvars["cat"] = None
	exec filt in gvars
	return gvars.get("cat", None)

class FLVFuseFile():
	def __init__(self, fuse, path, flags, *mode):
		self.entry = flv_resolve_path(path)
		self.cmd = pid_to_command(fuse.GetContext()["pid"])
		if isinstance(self.entry, FLVFile):
			self.file = file(self.entry.get_real_name())
			self.read = self.__read_flvfile
			self.release = self.__release_flvfile
			if self.cmd in ["mplayer"]:
				self.asc = add_asc_file(self.entry)
				timing.start()
			else:
				self.asc = None
		elif isinstance(self.entry, VirtualFile):
			# Mplayer may use virtual files as playlists.
			# Since file contents should not change when
			# mplayer is using it, we copy the contents
			# here instead of reading the latest data.

			# A hack to get mplayer's playlist compatible
			# contents.
			self.entry.mplayer = self.cmd in ["mplayer"]
			self.virt_data = self.entry.get_contents()
			self.read = self.__read_virtual
			# This causes recent-list's last item name to
			# be truncated.
			#del self.entry.mplayer
			lang = stats.lookup("language")
			dyncat = stats.lookup("dyncat")
			if self.entry == lang:
				self.write = self.__write_lang
			elif self.entry == dyncat:
				self.write = self.__write_dyncat

	def __read_flvfile(self, size, offset):
		self.file.seek(offset)
		return self.file.read(size)

	def __read_virtual(self, size, offset):
		return self.virt_data[offset:offset + size]

	def read(self, size, offset):
		raise IOError("Unknown file type")

	def __release_flvfile(self, flags):
		self.file.close()
		if self.cmd in ["mplayer"]:
			timing.finish()
			# watchtime is in seconds
			wt = (timing.milli() / 1000.0) + self.entry.get_watchtime()
			self.entry.set_watchtime(wt)
			self.entry.set_count(self.entry.get_count() + 1)
			stats.categorize(self.entry)
			stats.update_stat_lists(self.entry)
			if self.asc:
				del_asc_file(self.asc)

	def release(self, flags):
		pass

	def __write_lang(self, buf, offset):
		# Check that the string is valid: "xx,yy,zz"
		for elt in buf.rstrip().split(","):
			if len(elt) != 2:
				return -errno.ERANGE
		self.entry.set_contents(buf.rstrip())
		return len(buf)

	def __write_dyncat(self, buf, offset):
		# Create a new list of entries that pass the
		# filter given by the user.
		test_fn = compile(buf, "dyncat", "single")
		dyncat = self.entry
		dyncat.entries = []
		gvars = {}
		for entry in stats._get_entries():
			gvars["category"] = stats.get_category_name(entry)
			if filter_entry(entry, test_fn, gvars):
				dyncat.entries.insert(0, entry)
		return len(buf)

	def write(self, buf, offset):
		return -errno.EROFS


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

class FLVStatistics(FLVDirectory):
	def __init__(self, entries):
		FLVDirectory.__init__(self, "stat")
		self.add_entry("language", VirtualFile("fi,en,la,it"))
		self.add_entry("top-list", VirtualFile("", self._update_top_file))
		self.add_entry("last-list", VirtualFile("", self._update_last_file))
		self.add_entry("recent", VirtualFile("", self._update_recent_file))
		self.add_entry("statistics", VirtualFile("", self.update_counts))
		self.dyncat = VirtualFile("", self._update_dyncat)
		self.add_entry("dyncat", self.dyncat)

		self.total_wtime = 0
		self.total_time = 0
		self.entries = entries
		self._sort_entries()
		for ent in self.entries:
			self.categorize(ent)
			self.total_wtime = self.total_wtime + ent.get_watchtime()
			self.total_time = self.total_time + ent.get_length()

		self.rlist = []
		self.toplist = self.entries[:10]
		self.lastlist = self.entries[-10:]
		self.dyncat.entries = []

	def _sort_entries(self):
		tmp = [(ent.get_watchtime(), ent) for ent in self.entries]
		tmp = sorted(tmp, lambda a,b: b[0]-a[0])
		self.entries = [elt[1] for elt in tmp]

	def update_counts(self, fil):
		def timestr(time):
			return "%02d:%02d:%02d" % (time / 3600, (time % 3600) / 60, time % 60)
		time = self.total_wtime
		wtstr = "total watchtime: %s" % timestr(time)
		time = time / len(self.entries)
		astr = "average watchtime: %s" % timestr(time)
		time = self.total_time
		tstr = "total time: %s" % timestr(time)
		fil.set_contents("files: %d\n%s\n%s\n%s\n" \
					 % (len(self.entries), wtstr, tstr, astr))

	# These methods update the file contents just before they are used.
	def _update_helper(self, f, lst, mapper):
		if hasattr(f, "mplayer") and f.mplayer == True:
			mapper = self._entry_to_mplayer_str
		f.set_contents("".join(map(mapper, lst)))

	def _update_recent_file(self, rfile):
		self._update_helper(rfile, self.rlist, self._entry_to_recent_str)

	def _update_top_file(self, tfile):
		self._update_helper(tfile, self.toplist, self._entry_to_timed_str)

	def _update_last_file(self, lfile):
		self._update_helper(lfile, self.lastlist, self._entry_to_timed_str)

	def _update_dyncat(self, dfile):
		self._update_helper(dfile, dfile.entries, self._entry_to_timed_str)


	def _get_entries(self):
		return self.entries

	def _entry_to_recent_str(self, entry):
		(parent, name) = entry.get_pos()
		(_, pname) = parent.get_pos()
		return "%s%c%s\n" % (pname, os.path.sep, name)

	def _entry_to_timed_str(self, entry):
		time = entry.get_watchtime()
		(parent, name) = entry.get_pos()
		(_, pname) = parent.get_pos()
		return "%02d:%02d\t%s%c%s\n" % (time / 60, time % 60, pname, os.path.sep, name)

	def _entry_to_mplayer_str(self, entry):
		(parent, name) = entry.get_pos()
		(_, pname) = parent.get_pos()
		return "..%c%s%c%s\n" % (os.path.sep, pname, os.path.sep, name)

	# This method updates the lists that are used to create stat-files' contents.
	def update_stat_lists(self, entry):
		if not entry in self.rlist[:1]:
			self.rlist.insert(0, entry)
			self.rlist = self.rlist[:10] # max 10 items
		self._sort_entries()
		self.toplist = self.entries[:10]
		self.lastlist = self.entries[-10:]

	def get_category_name(self, entry):
		global filters
		for filt in filters:
			cat = filter_entry(entry, filt)
			if not cat is None:
				return cat
		return "unknown"

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
	global def_lang, filters, files
	cfg = file(filename)
	lines = cfg.readlines()
	cfg.close()
	for line in lines:
		if line[0] == "#": pass
		elif line[:9] == "language=":
			def_lang = line[9:].rstrip()
		elif line[:9] == "database=":
			FLVFile.db = FLVDatabase(line[9:].rstrip())
		elif line[:7] == "filter=":
			filters.append(compile(line[7:], filename, "single"))
		else:		# add files in directory (line ends with '\n')
			files.process(line[:-1], ".flv")

fuse.fuse_python_api = (0,2)
flv_root = None
stats = None
def_lang = None
all_entries = None
filters = None
files = None

def main():
	global flv_root, stats, def_lang, all_entries, filters, files
	flv_root = FLVDirectory("/")
	def_lang = "fi,en,la,it"
	all_entries = []
	filters = []
	files = Files()
	FLVFile.files = files	# XXX: Hack

	flv_parse_config_file("atrfs.conf")

	# Add files into root directory.
	for name in files.get_names():
		ent = FLVFile(name)
		flv_root.add_entry(name, ent)
		all_entries.insert(0, ent)

	stats = FLVStatistics(all_entries)
	flv_root.add_entry("stat", stats)
	stats.lookup("language").set_contents(def_lang)
	all_entries = None

	server = ATRFS()
	server.parse()
	server.main()

main()
