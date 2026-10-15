r"""DBXV2.sav encryption: decrypts a save to its plain body and encrypts a
changed body back the way the game expects.

Container (after Xv2CoreLib SAV\Encryption.cs): u32 +4 file size, i32 +8
section size, md5 of the section at +0x10, section at +0x20 = 0x80-byte
header (AES-CTR with the fixed key below) + body (AES-CTR with the key/iv the
header holds). CTR is its own inverse, so one function does both ways. The
plain body starts with '#SAV'.
Needs the `cryptography` package (pip install cryptography).
"""
import hashlib, struct
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes

KEY = b"PR]-<Q9*WxHsV8rcW!JuH7k_ug:T5ApX"      # fixed header key (Encryption.cs)
IV = b"_Y7]mD1ziyH#Ar=0"
SECTION_START, SECTION_HEADER = 0x20, 0x80


def ctr(data, key, iv):
    """AES-CTR; the same call encrypts and decrypts."""
    return Cipher(algorithms.AES(key), modes.CTR(iv)).decryptor().update(data)


def body_key(header):
    """The body's key and iv inside the decrypted header (offsets by header flag 5 & 4)."""
    key_at, iv_at = (0x4C, 0x6C) if header[5] & 4 else (0x1C, 0x3C)
    return header[key_at:key_at + 32], header[iv_at:iv_at + 16]


def decrypt(raw):
    """Plain save body of a DBXV2.sav file (returned as is if already plain)."""
    if raw[:4] == b"#SAV":
        return raw
    section_size = struct.unpack_from("<i", raw, 8)[0]
    section = raw[SECTION_START:SECTION_START + section_size]
    header = ctr(section[:SECTION_HEADER], KEY, IV)
    if header[:4] != b"#SAV":
        raise ValueError("not a DBXV2.sav file (header signature)")
    body = ctr(section[SECTION_HEADER:], *body_key(header))
    if body[:4] != b"#SAV":
        raise ValueError("not a DBXV2.sav file (body signature)")
    return body


def encrypt_like(template_raw, body):
    """Re-encrypts a decrypted save body with the key/iv of an existing file."""
    section_size = struct.unpack_from("<i", template_raw, 8)[0]
    section = template_raw[SECTION_START:SECTION_START + section_size]
    header = ctr(section[:SECTION_HEADER], KEY, IV)
    if len(body) != section_size - SECTION_HEADER:
        raise ValueError("body size differs from the template")
    encrypted_body = ctr(body, *body_key(header))
    # The game rejects a save as corrupt unless these header bytes match
    # (Xv2CoreLib EncryptSaveFile): 0x1A / 0x1B = byte sum of every 32nd byte
    # of the plain / encrypted body, 0x14 = header[5] + sum of 0x15..0x1B.
    header = bytearray(header)
    header[0x1A] = sum(body[::32]) & 0xFF
    header[0x1B] = sum(encrypted_body[::32]) & 0xFF
    header[0x14] = (header[5] + sum(header[0x15:0x1C])) & 0xFF
    new_section = ctr(bytes(header), KEY, IV) + encrypted_body
    raw = bytearray(template_raw)
    raw[SECTION_START:SECTION_START + section_size] = new_section
    raw[0x10:0x20] = hashlib.md5(new_section).digest()
    return bytes(raw)
