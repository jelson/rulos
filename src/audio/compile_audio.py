#!/usr/bin/python

import sys

class AudioClip:
	def __init__(self, start, length):
		self.start = start
		self.length = length

class AudioDotCH:
	def __init__(self, aufilenames):
		curOffset = 0
		self.indices = []
		self.outputBytes = ""
		for aufilename in aufilenames:
			sys.stderr.write("reading %s\n" % aufilename)
			fp = open(aufilename)
			# discard .au header
			fp.read(24)
			everything = fp.read(9999999)
			self.indices.append(AudioClip(curOffset, len(everything)))
			curOffset += len(everything)
			self.outputBytes += everything

	def emitIndex(self):
		sys.stdout.write("AudioClip audio_clips[] = {\n")
		for clip in self.indices:
			sys.stdout.write("	{ %d, %d },\n" % (clip.start, clip.length))
		sys.stdout.write("};\n")
		sys.stdout.write("uint8_t num_audio_clips = %d;\n" % len(self.indices))

#		sys.stdout.write("#ifdef SIM\n")
#		sys.stdout.write("uint8_t buf_base[] = {\n")
#		for byte in self.outputBytes:
#			sys.stdout.write("	0x%02x,\n" % ord(byte))
#		sys.stdout.write("};\n")
#		sys.stdout.write("#endif // SIM\n")

	def emitSpiflashBin(self):
		sys.stdout.write(self.outputBytes)

def parseAudioFilenames(includeFile, audio_path):
	fp = open(includeFile)
	reading = False
	names = []
	while (True):
		l = fp.readline()
		if (l==""): break
		l = l.strip()
		if (l=="// START_SOUND_TOKEN_ENUM"):
			reading = True
			continue
		if (l=="// END_SOUND_TOKEN_ENUM"):
			break
		if (reading):
			name = l.split(",")[0].split(" ")[0]
			names.append("%s/%s.au" % (audio_path, name))
	sys.stderr.write("decoded names: %s\n"%names)
	return names

def main():
	mode = sys.argv[1]
	enum_include = sys.argv[2]
	audio_path = sys.argv[3]
	outfile = AudioDotCH(parseAudioFilenames(enum_include, audio_path))
	if (mode=="-index"):
		outfile.emitIndex()
	elif (mode=="-spif"):
		outfile.emitSpiflashBin()
	else:
		assert(False)

main()
