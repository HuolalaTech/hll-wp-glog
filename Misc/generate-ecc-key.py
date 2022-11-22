#!/usr/bin/env python
from binascii import hexlify, unhexlify

import pyelliptic

CURVE = 'secp256k1'

svr = pyelliptic.ECC(curve=CURVE)
svr_pubkey = svr.get_pubkey()
svr_privkey = svr.get_privkey()

print "private key:[%s]" % hexlify(svr_privkey).upper()

print "\npublic key:[%s%s]" % (hexlify(svr.pubkey_x).upper(), hexlify(svr.pubkey_y).upper())
