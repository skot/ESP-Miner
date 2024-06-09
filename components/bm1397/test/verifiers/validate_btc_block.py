import hashlib
from binascii import unhexlify, hexlify

"""
Block #839900, data from https://blockchain.info/rawblock/000000000000000000023dfafae2b6e6b5ecf9d1365fafa075dec49625721f37
This script is based on https://github.com/Tennyx/validate-btc-block
"""
btc_version = 714498048
hex_prev_hash = "000000000000000000015d7eee767c24abd355f70fb382d2ef47398610439ba3"
hex_merkle_hash = "088083f58ddef995494fec492880da49e3463cc73dee1306dbdf6cf3af77454c"
epoch_time = 1713511391
bits = 386089497
nonce = 3529540887

def endian_big_to_little(hex):
    padded_hex = ''
    if len(hex) != 8:
        padding_len = 8 - len(hex)
        for num in range(padding_len):
            padded_hex += '0'
        padded_hex += str(hex)
    else:
        padded_hex = hex
    ba = bytearray.fromhex(padded_hex)
    ba.reverse()
    le_hex = ''.join(format(x, '02x') for x in ba)
    return le_hex

hex_btc_version = endian_big_to_little(hex(btc_version)[2:])
hex_prev_hash = endian_big_to_little(hex_prev_hash)
hex_merkle_hash = endian_big_to_little(hex_merkle_hash)
hex_time = endian_big_to_little(hex(int(epoch_time))[2:])
hex_bits = endian_big_to_little(hex(bits)[2:])
hex_nonce = endian_big_to_little(hex(nonce)[2:])

header_hex = (
    hex_btc_version +
    hex_prev_hash +
    hex_merkle_hash +
    hex_time +
    hex_bits +
    hex_nonce
)

header_bin = unhexlify(header_hex)
print("header:", hexlify(header_bin).decode("utf-8"))
hash = hashlib.sha256(hashlib.sha256(header_bin).digest()).digest()
print("hash:", hexlify(hash[::-1]).decode("utf-8"))

