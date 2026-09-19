'''
This is the source code of AyuGram for Desktop.

We do not and cannot prevent the use of our code,
but be respectful and credit the original author.

Copyright @Bush2021, 2026
'''
import glob, re, binascii, os, sys

sys.dont_write_bytecode = True
scriptPath = os.path.dirname(os.path.realpath(__file__))
sys.path.append(scriptPath + '/../../../../lib_tl/tl')
from generate_tl import generate

generate({
  'namespaces': {
    'creator': 'AyuSecret::details',
  },
  'prefixes': {
    'type': 'SecretTL',
    'data': 'SecretTLD',
    'id': 'secretc',
    'construct': 'secret_',
  },
  'types': {
    'prime': 'mtpPrime',
    'typeId': 'mtpTypeId',
    'buffer': 'mtpBuffer',
  },
  'sections': [
    'read-write',
  ],

  'flagInheritance': {
  },

  # TDLib suffixes the per-layer duplicates so that every constructor has a
  # unique name, and the generator recomputes the id from the name it reads.
  # decryptedMessageMediaDocument is listed for another reason: its id was
  # computed over the literal 'bytes' fields, not over the 'string' the
  # synonym rewrites them to. Every id here was checked against the crc32 of
  # the original line.
  'typeIdExceptions': [
    'decryptedMessage8#1f814f1f',
    'decryptedMessageService8#aa48327d',
    'decryptedMessageMediaPhoto8#32798a8c',
    'decryptedMessageMediaVideo8#4cee6ef3',
    'decryptedMessageMediaDocument8#b095434b',
    'decryptedMessageMediaAudio8#6080758f',
    'decryptedMessage23#204d3878',
    'decryptedMessageMediaVideo23#524a415d',
    'documentAttributeSticker23#fb0a5727',
    'documentAttributeVideo23#5910cccb',
    'documentAttributeAudio23#51448e5',
    'documentAttributeAudio45#ded218e0',
    'decryptedMessage46#36b091de',
    'decryptedMessageMediaDocument46#7afe8ae2',
    'decryptedMessageMediaDocument#6abd9782',
  ],

  'renamedTypes': {
  },

  'skip': [
    'int ? = Int;',
    'long ? = Long;',
    'double ? = Double;',
    'string ? = String;',

    'vector {t:Type} # [ t ] = Vector t;',
  ],
  'builtin': [
    'int',
    'long',
    'double',
    'string',
    'bytes',
  ],
  'builtinTemplates': [
    'vector',
    'flags',
  ],
  'synonyms': {
    'bytes': 'string',
  },
  'builtinInclude': 'ayu/secret/scheme/secret_tl_core.h',
  'optimizeSingleData': True,

})
