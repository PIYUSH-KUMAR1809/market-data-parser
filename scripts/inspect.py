import json

with open("../data/l2_data/capture_5000.jsonl") as f:
    for i, line in enumerate(f):
        data = json.loads(line)
        payload = bytes.fromhex(data["payload_hex"])
        print(f"Packet {i+1}: size={len(payload)}")
        print(f"  First 16 bytes: {payload[:16].hex(' ')}")
        if i == 4:
            break
