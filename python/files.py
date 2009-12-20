# -*- coding: utf-8 -*-

import os

class Files():
	def __init__(self):
		self.__entries = {}
		self.__dirs = []

	def __ext(self, name):
		if "." in name:
			i = name.rindex(".")
			return name[i:]
		return ""

	def add_real_file(self, real_file):
		sep = real_file.rindex(os.sep)
		directory = real_file[:sep]
		fname = real_file[sep + 1:]
		count = 0
		ext = self.__ext(fname)
		l = len(ext)
		name = fname
		while self.__entries.get(name, None):
			count = count + 1
			name = "%s_%d%s" % (name[:-l], count, ext)
		if directory not in self.__dirs:
			self.__dirs.append(directory)
		self.__entries[name] = (self.__dirs.index(directory), fname)

	def del_real_file(self, real_file):
		name = self.get_name (real_file)
		if name:
			del(self.__entries[name])

	def del_name(self, name):
		if self.__entries.get(name):
			del(self.__entries[name])

	def real_name(self, name):
		data = self.__entries.get(name)
		if data:
			(idx, fname) = data
			return "%s%c%s" % (self.__dirs[idx], os.sep, fname)
		return None

	def get_name(self, real_file):
		sep = real_file.rindex(os.sep)
		fname = real_file[sep + 1:]
		directory = real_file[:sep]
		idx = self.__dirs.index(directory)
		count = 0
		ext = self.__ext(fname)
		l = len(ext)
		name = fname
		while self.__entries.get(name, None):
			if self.__entries.get(name) == (idx, fname):
				return name
			count = count + 1
			name = "%s_%d%s" % (name[:-l], count, ext)
		return None

	def get_names(self):
		return self.__entries.keys()

	def process(self, name, ext=".flv", from_scratch = False):
		if from_scratch:
			self.__entries = {}
		l = len(ext)
		for d, sd, fnames in os.walk(name):
			for n in filter(lambda (n): n[-l:] == ext, fnames):
				self.add_real_file("%s%c%s" % (d, os.sep, n))
