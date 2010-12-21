#!/usr/bin/python

import sys
import re
import os.path

AO_HALFBUFLEN = 128
	# TODO fetch from source audio_out.h AO_HALFBUFLEN
SDBLOCKSIZE = 512
index_size_blocks = 1
MAGIC = "RULOSMAGICSD"

class AudioClip:
	def __init__(self, fn, aubytes):
		self.fn = fn
		self.aubytes = aubytes

	def __len__(self):
		return len(self.aubytes)

	def round(self, align):
		if (len(self)==0): return 0
		return int((len(self)-1)/align+1)*align

	def aolen(self):
		return self.round(AO_HALFBUFLEN)

	def sdlen(self):
		return self.round(SDBLOCKSIZE)

	def preroll(self):
		return self.sdlen() - self.aolen()

	def __repr__(self):
		return "len %6x aolen %6x sdlen %6x preroll %6x" % (
			len(self),
			self.aolen(),
			self.sdlen(),
			self.preroll())

	def sdcardEncoding(self):
		postroll = self.aolen() - len(self)
		return ("\0"*self.preroll()) + self.aubytes + ("\x7f"*postroll)

def avr_int32(x):
	s = ""
	s+=chr((x>> 0)&0xff)
	s+=chr((x>> 8)&0xff)
	s+=chr((x>>16)&0xff)
	s+=chr((x>>24)&0xff)
	return s

def avr_int16(x):
	s = ""
	s+=chr((x>> 0)&0xff)
	s+=chr((x>> 8)&0xff)
	return s

class IndexRecord:
	def __init__(self, clip, offset):
		self.clip = clip
		self.offset = offset

	def end_offset(self):
		return self.offset + self.clip.sdlen()

	def __repr__(self):
		return "offset %8x end %8x preroll %4x" % (
			self.offset,
			self.end_offset(),
			self.clip.preroll())
	def dataBytes(self):
		record = ""
		record += avr_int32(self.offset)
		record += avr_int32(self.end_offset())
		record += avr_int16(self.clip.preroll())
		is_disco = self.clip.fn.startswith("../audio/disco")
		record += avr_int16(is_disco)
		return record

class AudioDotCH:
	def __init__(self, aufilenames):
		self.clips = []
		for aufilename in aufilenames:
			sys.stderr.write("reading %s\n" % aufilename)
			fp = open(aufilename)
			# discard .au header
			fp.read(24)
			everything = fp.read(9999999)
			self.clips.append(AudioClip(aufilename, everything))

	def checkCard(self, cardpath):
		fd = open(cardpath, "r")
		magic = fd.read(len(MAGIC))
		if (magic != MAGIC):
			raise Exception("Card magic invalid. Not writing to avoid stomping real data.")
		fd.close()

	def emitSDCard(self, cardpath):
		self.checkCard(cardpath)
		cardfd = open(cardpath, "w")

		index = MAGIC
		index_size_bytes = index_size_blocks * SDBLOCKSIZE
		offset = index_size_bytes
		for clip in self.clips:
			ir = IndexRecord(clip, offset)
			index += ir.dataBytes()
			offset = ir.end_offset()
			sys.stderr.write(repr(clip)+"\n")
			sys.stderr.write("    "+repr(ir)+"\n")
		assert(index_size_bytes >= len(index))
		pad = "\0"*(index_size_bytes - len(index))
		cardfd.write(index+pad)
		sys.stderr.write("index len 0x%x\n" % len(index))
		for clip in self.clips:
			cardfd.write(clip.sdcardEncoding())
		cardfd.close()

class ParseAudioFilenames:
	def __init__(self, includeFile, audio_path):
		self._names = []
		self.process(includeFile, audio_path)

	def names(self):
		return self._names

	def process(self, includeFile, audio_path, reading = False):
		sys.stderr.write("processing %s\n" % includeFile)
		fp = open(includeFile)
		while (True):
			l = fp.readline()
			if (l==""): break
			l = l.strip()

			imo = re.compile('#include "(.*)"').search(l)
			if (imo!=None):
				if (not reading):
					continue
				include = imo.groups(1)[0]
				path = os.path.dirname(include)
#				path = path.split("/")
#				path = path[2:]
#				path = "/".join(path)
				sys.stderr.write("including %s\n" % include)
				self.process(include, path, reading = True)
				continue
			if (l=="// START_SOUND_TOKEN_ENUM"):
				reading = True
				continue
			if (l=="// END_SOUND_TOKEN_ENUM"):
				break
			if (reading):
				name = l.split(",")[0].split(" ")[0]
				self._names.append("%s/%s.au" % (audio_path, name))
		sys.stderr.write("decoded names: %s\n" % self._names)
		fp.close()

def main():
	mode = sys.argv[1]
	enum_include = sys.argv[2]
	audio_path = sys.argv[3]
	cardpath = sys.argv[4]
	outfile = AudioDotCH(ParseAudioFilenames(enum_include, audio_path).names())
	if (mode=="-index"):
		outfile.emitIndex()
	elif (mode=="-spif"):
		outfile.emitSpiflashBin()
	elif (mode=="-sdcard"):
		outfile.emitSDCard(cardpath)
	else:
		assert(False)

main()
