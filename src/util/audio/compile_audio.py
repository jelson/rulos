#!/usr/bin/env python3

import sys
import re
import os.path
import glob
import subprocess

AO_HALFBUFLEN = 128
    # TODO fetch from source audio_out.h AO_HALFBUFLEN
SDBLOCKSIZE = 512
index_size_blocks = 1
MAGIC = "RULOSMAGICSD"
AUDIO_RATE=50000
AUDIO_FILTERS={
    'filter_none': '',
#    'filter_music': 'gain -6 bass -20',
#    'filter_music': 'bass -20',
#    'filter_music': 'highpass 340',
    'filter_music': '',
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
    def __init__(self, tokens, audio_root):
        self.audio_root = audio_root
        self.clips = []
        for token in tokens:
            sys.stderr.write("reading %s\n" % token.symbol)
            sys.stderr.write("cmd: %s\n" % self.filter(token))
            completed = subprocess.run(self.filter(token), shell=True, capture_output=True)
            # discard .au header
            everything = completed.stdout[24:]
            if len(everything) == 0:
                raise Exception("filter gave empty output")
            self.clips.append(AudioClip(token, everything))

    def filter(self, token):
        audio_dir = os.path.join(self.audio_root, token.outdir)
        # You can test play a rocket-raw-encoded file on the command line with:
        # play --channels 2 -e signed-integer --rate 50000 -t raw --bits 16 <filename.raw>
        cmd = "sox %s --rate %s --channels 2 -t raw --bits 16 -e signed-integer - %s" % (
            token.source_path,
            AUDIO_RATE,
            AUDIO_FILTERS[token.filter_name])
        return cmd

    def checkCard(self, cardpath):
        fd = open(cardpath, "r")
        magic = fd.read(len(MAGIC))
        if (magic != MAGIC):
            raise Exception("Card magic invalid. Not writing to avoid stomping real data.")
        fd.close()

    def emitFatfs(self, cardpath):
            os.mkdir(cardpath)
            for clip in self.clips:
                outpath = os.path.join(cardpath, clip.token.outdir)
                try:
                    os.mkdir(outpath)
                except OSError:    # File exists
                    pass
                outpath = os.path.join(outpath, clip.token.fat_filename())
                print(f"Writing to {outpath}")
                fp = open(outpath, "wb")
                fp.write(clip.aubytes)
                fp.close()

        # This emits an old rocket-specific hardcoded filesystem. Now that we've got FAT,
        # this is dead code. Hooray!
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
    def __init__(self, outdir, idx, source_path, symbol, source_file_name, filter_name, label):
        self.outdir = outdir
        self.idx = idx
        self.source_path = source_path
        self.symbol = symbol
        self.source_file_name = source_file_name
        self.filter_name = filter_name
        self.label = label

    def fat_filename(self):
        if self.outdir == "sfx":
            return "%05d.raw" % self.idx
        else:
            return self.symbol

    def __repr__(self):
        return self.symbol

class ParseAudioFilenames:
    def __init__(self, outdir, includeFile, audio_root):
        self._tokens = []
        self.process(outdir, includeFile, audio_root)

    def tokens(self):
        return self._tokens

    def process(self, outdir, includeFile, audio_root):
        sys.stderr.write("processing %s\n" % includeFile)
        fp = open(includeFile)
        idx = 0
        fx_root = os.path.join(audio_root, "fx")
        while (True):
            l = fp.readline()
            if (l==""): break
            l = l.strip()

            mo = re.compile('SOUND\((.*)\)').search(l)
            if (mo==None):
                continue
            args = mo.groups(1)[0]
            words = map(lambda s: s.strip(), args.split(','))
            print(words)
            (symbol, source_file_name, filter_name, label) = words
            self._tokens.append(Token(outdir, idx, os.path.join(fx_root, source_file_name),
                symbol, source_file_name, filter_name, label))
            idx += 1
        sys.stderr.write("decoded names: %s\n" % self._tokens)
        fp.close()

def FindMusicTokens(audio_root):
    music_dir = os.path.join(audio_root, "disco")
    tokens = []
    for path in glob.glob(os.path.join(music_dir, "*")):
        if path.endswith(".ogg") or path.endswith(".mp3"):
            music_filename = os.path.split(path)[-1]
            symbol_name = music_filename[:-4] + ".raw"
            tokens.append(Token("music", -1, os.path.join(music_dir, music_filename), symbol_name, music_filename, 'filter_music', 'label_disco'))
    return tokens

def main():
    mode = sys.argv[1]
    enum_include = sys.argv[2]
    audio_root = sys.argv[3]

    try:
        cardpath = sys.argv[4]
    except:
        cardpath = None

    sfxTokens = ParseAudioFilenames("sfx", enum_include, audio_root).tokens()
    musicTokens = FindMusicTokens(audio_root)
    outfile = AudioIndexer(sfxTokens + musicTokens, audio_root)
    if (mode=="-index"):
        outfile.emitIndex()
    elif (mode=="-spif"):
        outfile.emitSpiflashBin()
    elif (mode=="-sdcard"):
        outfile.emitSDCard(cardpath)
    elif (mode=="-fatfs"):
        outfile.emitFatfs(cardpath)
    else:
        assert(False)

main()
