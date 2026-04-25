import struct

def create_system_event(code, tracking):
    msg_type = b'S'
    stock_locate = 0
    timestamp = 123456789
    event_code = code.encode('ascii')
    ts_bytes = struct.pack('>Q', timestamp)[2:] 
    payload = struct.pack('>cHH', msg_type, stock_locate, tracking) + ts_bytes + struct.pack('>c', event_code)
    length = len(payload)
    return struct.pack('>H', length) + payload

def create_stock_directory(stock, loc, tracking):
    msg_type = b'R'
    timestamp = 123456789
    ts_bytes = struct.pack('>Q', timestamp)[2:]
    stock_bytes = stock.ljust(8).encode('ascii')
    payload = struct.pack('>cHH', msg_type, loc, tracking) + ts_bytes + stock_bytes + (b'\x00' * 20)
    length = len(payload)
    return struct.pack('>H', length) + payload

def create_add_order(ref, stock_loc, shares, price, tracking):
    msg_type = b'A'
    timestamp = 123456789
    ts_bytes = struct.pack('>Q', timestamp)[2:]
    payload = struct.pack('>cHH', msg_type, stock_loc, tracking) + ts_bytes + \
              struct.pack('>Q', ref) + b'B' + struct.pack('>I', shares) + \
              b'AAPL    ' + struct.pack('>I', price)
    length = len(payload)
    return struct.pack('>H', length) + payload

with open('data/test_data.bin', 'wb') as f:
    t = 1
    f.write(create_system_event('O', t)); t+=1
    f.write(create_stock_directory('AAPL', 1, t)); t+=1
    f.write(create_stock_directory('GOOGL', 2, t)); t+=1
    
    for i in range(100):
        f.write(create_add_order(1000+i, 1, 100, 1500000, t))
        t += 1

    f.write(create_system_event('C', t)); t+=1
    
print("Created data/test_data.bin")
