#!/usr/bin/python

import sys
import re
import os.path

AO_HALFBUFLEN = 128
	# TODO fetch from source audio_out.h AO_HALFBUFLEN
SDBLOCKSIZE = 512
index_size_blocks = 1
MAGIC = "RULOSMAGICSD"
AUDIO_RATE=12000
AUDIO_FILTERS={
	'filter_none': '',
#	'filter_music': 'gain -6 bass -20',
	'filter_music': 'bass -20',
	}

class AudioClip:
	def __init__(self, token, aubytes):
		self.token = token
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
		return ("\0"*self.preroll()) + self.aubytes + ("\0"*postroll)

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
		is_disco = self.clip.token.label == "label_disco"
		record += avr_int16(is_disco)
		return record

class AudioIndexer:
	def __init__(self, tokens):
		self.clips = []
		for token in tokens:
			sys.stderr.write("reading %s\n" % token.symbol)
			sys.stderr.write("cmd: %s\n" % self.filter(token))
			fp = os.popen(self.filter(token), "r")
			# discard .au header
			fp.read(24)
			everything = fp.read(9999999)
			fp.close()
			self.clips.append(AudioClip(token, everything))

	def filter(self, token):
		cmd = "sox %s --bits 8 --rate %s --channels 1 -t au --bits 8 -e signed-integer - %s" % (
			token.source_file_name,
			AUDIO_RATE,
			AUDIO_FILTERS[token.filter_name])
		return cmd

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

class Token:
	def __init__(self, symbol, source_file_name, filter_name, label):
		self.symbol = symbol
		self.source_file_name = source_file_name
		self.filter_name = filter_name
		self.label = label

	def __repr__(self):
		return self.symbol

class ParseAudioFilenames:
	def __init__(self, includeFile, audio_path):
		self._tokens = []
		self.process(includeFile, audio_path)

	def tokens(self):
		return self._tokens

	def process(self, includeFile, audio_path, reading = False):
		sys.stderr.write("processing %s\n" % includeFile)
		fp = open(includeFile)
		while (True):
			l = fp.readline()
			if (l==""): break
			l = l.strip()

			mo = re.compile('SOUND\((.*)\)').search(l)
			if (mo==None):
				continue
			args = mo.groups(1)[0]
			words = map(lambda s: s.strip(), args.split(','))
			print words
			self._tokens.append(Token(*words))
		sys.stderr.write("decoded names: %s\n" % self._tokens)
		fp.close()

def main():
	mode = sys.argv[1]
	enum_include = sys.argv[2]
	audio_path = sys.argv[3]
	cardpath = sys.argv[4]
	outfile = AudioIndexer(ParseAudioFilenames(enum_include, audio_path).tokens())
	if (mode=="-index"):
		outfile.emitIndex()
	elif (mode=="-spif"):
		outfile.emitSpiflashBin()
	elif (mode=="-sdcard"):
		outfile.emitSDCard(cardpath)
	else:
		assert(False)

main()
