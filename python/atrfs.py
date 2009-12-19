#!/usr/bin/env python
# -*- coding: utf-8 -*-
# atrfs.py - 1.5.2009 - 19.12.2009 Ari & Tero Roponen

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

class SubtitleFile(VirtualFile):
	def __init__(self, flv_entry):
		VirtualFile.__init__(self, "")
		(parent, flv_name) = flv_entry.get_pos()
		length = flv_entry.get_length()
		val = 1.0 * flv_entry.get_watchtime() / length
		timestr = "%02d:%02d" % (length / 60, length % 60)

		# Try to find real subtitles.
		flv = flv_entry.get_real_name()
		asc = "%s.asc" % flv[:-4] # ".flv"
		text = None
		for lang in stats.lookup("language").get_contents().split(","):
			if not text:
				try:
					text = asc_read_subtitles(asc, lang, True) # As list.
				except IOError, e:
					pass

		if text:
			# Found subtitles. Insert statistics at the beginning.
			# 0\n<timestamp>\n<stat here>\n<1. line>\n<2.line>\n...
			text.insert(2 + text.index("0\n"), "%2.2f × %s\n" % (val, timestr))
			text = "".join(text)
		else:
			# Not found. Create fake subtitles.
			title = "%s\n%2.2f × %s" % (flv_name[:-4], val, timestr)
			singer = flv_entry.get_attr("user.singer", None)
			text = asc_fake_subtitles(title, length, singer)

		self.set_contents(text)


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
				(parent, name) = self.entry.get_pos()
				srt_name = "%s.srt" % name[:-4]
				self.srt = SubtitleFile(self.entry)
				parent.add_entry(srt_name,self.srt)
				timing.start()
			else:
				self.srt = None
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
			if self.entry == stats.lookup("language"):
				self.write = self.__write_lang
			elif self.entry == stats.lookup("dyncat"):
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
			if self.srt:
				(parent, name) = self.srt.get_pos()
				parent.del_entry(name)
				self.srt = None

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

class TopListFile(VirtualFile):
	def __init__(self):
		VirtualFile.__init__(self, "")
		self.top_list = []

	def update_contents(self):
		stats.update_helper(self, self.top_list, stats.entry_to_timed_str)

	def refresh_list(self, new_list):
		self.top_list = new_list

class LastListFile(VirtualFile):
	def __init__(self):
		VirtualFile.__init__(self, "")
		self.last_list = []

	def update_contents(self):
		stats.update_helper(self, self.last_list, stats.entry_to_timed_str)

	def refresh_list(self, new_list):
		self.last_list = new_list

class RecentListFile(VirtualFile):
	def __init__(self):
		VirtualFile.__init__(self, "")
		self.__recent = []

	def update_contents(self):
		stats.update_helper(self, self.__recent, self.__entry_to_recent_str)

	def __entry_to_recent_str(self, entry):
		(parent, name) = entry.get_pos()
		(_, pname) = parent.get_pos()
		return "%s%c%s\n" % (pname, os.path.sep, name)

	def add_recent(self, entry):
		if not entry in self.__recent:
			self.__recent.insert(0, entry)
			self.__recent = self.__recent[:10] # Max 10 items.

class StatisticsFile(VirtualFile):
	def __init__(self):
		VirtualFile.__init__(self, "")
		self.__total_time = 0
		self.__watch_time = 0

	def update_contents(self):
		def timestr(time):
			return "%02d:%02d:%02d" % (time / 3600, (time % 3600) / 60, time % 60)
		l = len(stats.get_entries())
		time = self.__watch_time
		wtstr = "total watchtime: %s" % timestr(time)
		time = time / l
		astr = "average watchtime: %s" % timestr(time)
		time = self.__total_time
		tstr = "total time: %s" % timestr(time)
		self.set_contents("files: %d\n%s\n%s\n%s\n" % (l, wtstr, tstr, astr))

	def update_music_times(self, total_secs, watch_secs):
		self.__total_time = self.__total_time + total_secs
		self.__watch_time = self.__watch_time + watch_secs

class DyncatFile(VirtualFile):
	def __init__(self):
		VirtualFile.__init__(self, "")
		self.entries = []

	def update_contents(self):
		stats.update_helper(self, self.entries, stats.entry_to_timed_str)
	

class FLVStatistics(FLVDirectory):
	def __init__(self, entries):
		FLVDirectory.__init__(self, "stat")
		self.add_entry("language", VirtualFile("fi,en,la,it"))
		self.add_entry("top-list", TopListFile())
		self.add_entry("last-list", LastListFile())
		self.add_entry("recent", RecentListFile())
		self.add_entry("statistics", StatisticsFile())
		self.add_entry("dyncat", DyncatFile())

		self.__entries = entries
		self.__sort_entries()

		stat = self.lookup("statistics")
		for ent in self.get_entries():
			self.categorize(ent)
			stat.update_music_times(ent.get_length(), ent.get_watchtime())

		self.lookup("top-list").refresh_list(self.get_entries()[:10])
		self.lookup("last-list").refresh_list(self.get_entries()[-10:])

	def __sort_entries(self):
		tmp = [(ent.get_watchtime(), ent) for ent in self.get_entries()]
		tmp = sorted(tmp, lambda a,b: b[0]-a[0])
		self.__entries = [elt[1] for elt in tmp]

	def get_entries(self):
		return self.__entries

	# A helper method.
	def update_helper(self, f, lst, mapper):
		if hasattr(f, "mplayer") and f.mplayer == True:
			mapper = self.__entry_to_mplayer_str
		f.set_contents("".join(map(mapper, lst)))

	def entry_to_timed_str(self, entry):
		time = entry.get_watchtime()
		(parent, name) = entry.get_pos()
		(_, pname) = parent.get_pos()
		return "%02d:%02d\t%s%c%s\n" % (time / 60, time % 60, pname, os.path.sep, name)

	def __entry_to_mplayer_str(self, entry):
		(parent, name) = entry.get_pos()
		(_, pname) = parent.get_pos()
		return "..%c%s%c%s\n" % (os.path.sep, pname, os.path.sep, name)

	# This method updates the lists that are used to create stat-files' contents.
	def update_stat_lists(self, entry):
		self.__sort_entries()
		self.lookup("recent").add_recent(entry)
		self.lookup("top-list").refresh_list(self.get_entries()[:10])
		self.lookup("last-list").refresh_list(self.get_entries()[-10:])

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
