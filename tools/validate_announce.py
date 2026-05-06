#!/usr/bin/env python3
"""
Validate a microReticulum announce packet using Python Reticulum's crypto.

Usage:
    python3 validate_announce.py <hex-encoded-raw-packet>

The hex string is the full Reticulum packet (flags + hops + dest_hash + context + data),
as printed by the [ANN-WIRE] log line from Ratcom's serial output.

This script does NOT require a running Reticulum instance. It uses the pure Python
Ed25519/SHA-256 implementations directly from the reference codebase.
"""

import sys
import os
import hashlib

# Add the Reticulum reference implementation to the path. Override with
# RETICULUM_PY_PATH when validating against a different checkout.
DEFAULT_RETICULUM_PATH = os.path.abspath(
    os.path.join(os.path.dirname(__file__), "..", "..", "upstream", "Reticulum")
)
RETICULUM_PATH = os.environ.get("RETICULUM_PY_PATH", DEFAULT_RETICULUM_PATH)
sys.path.insert(0, RETICULUM_PATH)

from RNS.Cryptography.Ed25519 import Ed25519PublicKey

KEYSIZE = 512          # bits (32 bytes X25519 + 32 bytes Ed25519)
SIGLENGTH = 512        # bits (64 bytes)
NAME_HASH_LENGTH = 80  # bits (10 bytes)
RATCHETSIZE = 256      # bits (32 bytes)
TRUNCATED_HASHLENGTH = 128  # bits (16 bytes)
HEADER_MINSIZE = 19    # bytes


def sha256(data: bytes) -> bytes:
    return hashlib.sha256(data).digest()


def full_hash(data: bytes) -> bytes:
    return sha256(data)


def truncated_hash(data: bytes) -> bytes:
    return full_hash(data)[:TRUNCATED_HASHLENGTH // 8]


def validate_announce(raw_hex: str):
    raw = bytes.fromhex(raw_hex)
    print(f"Raw packet: {len(raw)} bytes")

    if len(raw) < HEADER_MINSIZE:
        print(f"FAIL: Packet too short ({len(raw)} < {HEADER_MINSIZE})")
        return False

    # === Unpack header ===
    flags = raw[0]
    hops = raw[1]

    header_type    = (flags & 0b01000000) >> 6
    context_flag   = (flags & 0b00100000) >> 5
    transport_type = (flags & 0b00010000) >> 4
    dest_type      = (flags & 0b00001100) >> 2
    packet_type    = (flags & 0b00000011)

    print(f"\n=== HEADER ===")
    print(f"  flags:          0x{flags:02x} (binary: {flags:08b})")
    print(f"  header_type:    {header_type} ({'HEADER_1' if header_type == 0 else 'HEADER_2'})")
    print(f"  context_flag:   {context_flag} ({'FLAG_SET (ratchet)' if context_flag else 'FLAG_UNSET'})")
    print(f"  transport_type: {transport_type} ({'BROADCAST' if transport_type == 0 else 'TRANSPORT'})")
    print(f"  dest_type:      {dest_type} ({['SINGLE','GROUP','PLAIN','LINK'][dest_type]})")
    print(f"  packet_type:    {packet_type} ({['DATA','ANNOUNCE','LINKREQUEST','PROOF'][packet_type]})")
    print(f"  hops:           {hops}")

    if packet_type != 1:
        print(f"FAIL: Not an ANNOUNCE packet (type={packet_type})")
        return False

    DST_LEN = TRUNCATED_HASHLENGTH // 8  # 16

    if header_type == 1:  # HEADER_2
        transport_id = raw[2:2+DST_LEN]
        destination_hash = raw[2+DST_LEN:2+2*DST_LEN]
        context = raw[2+2*DST_LEN]
        data = raw[2+2*DST_LEN+1:]
        print(f"  transport_id:   {transport_id.hex()}")
    else:  # HEADER_1
        transport_id = None
        destination_hash = raw[2:2+DST_LEN]
        context = raw[2+DST_LEN]
        data = raw[2+DST_LEN+1:]

    print(f"  destination:    {destination_hash.hex()}")
    print(f"  context:        0x{context:02x}")
    print(f"  data length:    {len(data)} bytes")

    # === Extract announce fields ===
    keysize = KEYSIZE // 8         # 64
    name_hash_len = NAME_HASH_LENGTH // 8  # 10
    sig_len = SIGLENGTH // 8       # 64
    ratchetsize = RATCHETSIZE // 8  # 32

    print(f"\n=== ANNOUNCE FIELDS ===")

    if len(data) < keysize + name_hash_len + 10 + sig_len:
        print(f"FAIL: Announce data too short ({len(data)} < {keysize + name_hash_len + 10 + sig_len})")
        return False

    public_key = data[:keysize]
    name_hash = data[keysize:keysize + name_hash_len]
    random_hash = data[keysize + name_hash_len:keysize + name_hash_len + 10]

    print(f"  public_key({len(public_key)}):  {public_key.hex()}")
    print(f"    X25519 enc:   {public_key[:32].hex()}")
    print(f"    Ed25519 sig:  {public_key[32:].hex()}")
    print(f"  name_hash({len(name_hash)}):  {name_hash.hex()}")
    print(f"  random_hash({len(random_hash)}): {random_hash.hex()}")
    print(f"    random part:  {random_hash[:5].hex()}")
    print(f"    timestamp:    {int.from_bytes(random_hash[5:], 'big')}")

    ratchet = b""
    if context_flag == 1:
        ratchet = data[keysize + name_hash_len + 10:keysize + name_hash_len + 10 + ratchetsize]
        signature = data[keysize + name_hash_len + 10 + ratchetsize:keysize + name_hash_len + 10 + ratchetsize + sig_len]
        app_data = data[keysize + name_hash_len + 10 + ratchetsize + sig_len:]
        print(f"  ratchet({len(ratchet)}):   {ratchet.hex()}")
    else:
        signature = data[keysize + name_hash_len + 10:keysize + name_hash_len + 10 + sig_len]
        app_data = data[keysize + name_hash_len + 10 + sig_len:]

    print(f"  signature({len(signature)}): {signature.hex()}")

    if len(app_data) > 0:
        print(f"  app_data({len(app_data)}):  {app_data.hex()}")
        try:
            print(f"    (text):       {app_data!r}")
        except:
            pass
    else:
        print(f"  app_data:       (none)")

    if not (len(data) > keysize + name_hash_len + 10 + sig_len):
        app_data = b""
        print(f"  (app_data cleared — packet not long enough)")

    # === Reconstruct signed_data (exactly as Python does) ===
    signed_data = destination_hash + public_key + name_hash + random_hash + ratchet + app_data

    print(f"\n=== SIGNATURE VERIFICATION ===")
    print(f"  signed_data({len(signed_data)}): {signed_data.hex()}")

    # Load Ed25519 public key (second 32 bytes of the 64-byte public key)
    ed25519_pub_bytes = public_key[32:]
    try:
        ed_pub = Ed25519PublicKey.from_public_bytes(ed25519_pub_bytes)
        try:
            ed_pub.verify(signature, signed_data)
            print(f"  SIGNATURE:      VALID")
            sig_ok = True
        except Exception as e:
            print(f"  SIGNATURE:      *** INVALID *** ({e})")
            sig_ok = False
    except Exception as e:
        print(f"  Ed25519 key load failed: {e}")
        sig_ok = False

    # === Destination hash verification ===
    print(f"\n=== DESTINATION HASH VERIFICATION ===")
    identity_hash = truncated_hash(public_key)
    print(f"  identity_hash:  {identity_hash.hex()}")

    hash_material = name_hash + identity_hash
    print(f"  hash_material({len(hash_material)}): {hash_material.hex()}")

    expected_hash = full_hash(hash_material)[:TRUNCATED_HASHLENGTH // 8]
    print(f"  expected_hash:  {expected_hash.hex()}")
    print(f"  packet_hash:    {destination_hash.hex()}")

    if destination_hash == expected_hash:
        print(f"  DEST HASH:      MATCH")
        hash_ok = True
    else:
        print(f"  DEST HASH:      *** MISMATCH ***")
        hash_ok = False

    # === Cross-check: what Python would compute for "lxmf.delivery" ===
    print(f"\n=== CROSS-CHECK ===")
    expected_name_hash = full_hash(b"lxmf.delivery")[:NAME_HASH_LENGTH // 8]
    print(f"  Expected name_hash for 'lxmf.delivery': {expected_name_hash.hex()}")
    print(f"  Packet name_hash:                       {name_hash.hex()}")
    if expected_name_hash == name_hash:
        print(f"  NAME HASH:      MATCH (confirms lxmf.delivery aspect)")
    else:
        print(f"  NAME HASH:      *** MISMATCH *** (different aspect or encoding)")

    # === Final result ===
    print(f"\n{'='*50}")
    if sig_ok and hash_ok:
        print(f"RESULT: ANNOUNCE IS VALID")
        print(f"Python Reticulum should accept this announce.")
    elif not sig_ok and hash_ok:
        print(f"RESULT: SIGNATURE FAILED (hash OK)")
        print(f"The Ed25519 signature doesn't verify. This means either:")
        print(f"  1. The signed_data differs from what was actually signed")
        print(f"  2. The Ed25519 implementation is incompatible")
        print(f"  3. The public key or signature bytes are corrupted")
    elif sig_ok and not hash_ok:
        print(f"RESULT: HASH MISMATCH (signature OK)")
        print(f"The destination hash doesn't match. This means:")
        print(f"  1. name_hash computation differs")
        print(f"  2. identity_hash computation differs")
        print(f"  3. full_hash or truncated_hash differs")
    else:
        print(f"RESULT: BOTH SIGNATURE AND HASH FAILED")
        print(f"Fundamental protocol mismatch.")

    return sig_ok and hash_ok


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(__doc__)
        print("Error: provide hex-encoded raw packet as argument")
        sys.exit(1)

    hex_input = sys.argv[1].strip().replace(" ", "").replace("\n", "")
    result = validate_announce(hex_input)
    sys.exit(0 if result else 1)
