"""Conditional instruction budget for the inspected Open Watcom bh_frame listing.

Intel 240440-002 (Nov 1989), table 10.1, assumptions and note 3.
This is not a cycle-accurate simulator or a real-time certification. Branch
choices are unconstrained (safe-path upper estimates may be unreachable).
Code/patch/stack are assumed cached; history-miss costs are explicit scenarios.
"""
import argparse
import functools
import hashlib
import json
import re
import sys
sys.setrecursionlimit(5000)

ap = argparse.ArgumentParser()
ap.add_argument("listing")
ap.add_argument("--json", required=True)
ap.add_argument("--room", action="store_true")
args = ap.parse_args()
raw = open(args.listing, "rb").read()
text = raw.decode("utf-8").replace("\r\n", "\n")
body = text.split("bh_room_frame_:\n" if args.room else "bh_frame_:\n", 1)[1].split("bh_room_render_:" if args.room else "bh_render_:", 1)[0]
instructions, labels = [], {}
for line in body.splitlines():
    line = line.strip()
    if not line:
        continue
    if line.endswith(":"):
        labels[line[:-1]] = len(instructions)
    else:
        parts = line.split(None, 1)
        instructions.append((parts[0].lower(), parts[1] if len(parts) > 1 else ""))

def target(operand):
    return labels[operand.split()[-1]]

backedges = []
for i, (op, operand) in enumerate(instructions):
    if op == "jmp" and target(operand) < i:
        prev_op, prev_arg = instructions[i - 1]
        if prev_op == "add" and prev_arg.lower().endswith(",18h"):
            backedges.append((i, target(operand)))
if len(backedges) != (0 if args.room else 1):
    raise ValueError("Expected exactly one packet-pointer loop; inspect new compiler output")
loop_jump, loop_head = backedges[0] if backedges else (-2, -2)
if not args.room and (instructions[loop_head][0] != "cmp" or instructions[loop_head + 1][0] != "je"):
    raise ValueError("Unrecognized packet loop guard")

def cost(op, operand, upper, taken=False):
    mem = "[" in operand
    dst = operand.split(",")[0]
    if op == "imul":
        # Every multiplier in this bounded kernel fits a signed 16-bit magnitude.
        # Table range 13..26 is a conservative envelope for these operands.
        base = 26 if upper else 13
    elif op == "div":
        base = 40
    elif op == "cdq":
        base = 3
    elif op in ("adc", "sbb"):
        base = 2 if mem else 1
    elif op == "mov":
        base = 2 if upper and mem else 1
    elif op in ("movsx", "movzx"):
        base = 3
    elif op == "lea":
        base = 2 if "+" in operand or "*" in operand else 1
    elif op in ("shl", "shr", "sar"):
        amount = operand.split(",")[-1].strip().lower()
        base = 3 if amount in ("cl", "1") else 2
    elif op in ("add", "sub", "and", "or", "xor", "cmp", "test", "inc", "dec", "neg"):
        base = (3 if "[" in dst and op not in ("cmp", "test") else 2) if mem else 1
    elif op == "push":
        base = 4 if mem else 1
    elif op == "pop":
        base = 2 if mem else 1
    elif op == "jmp":
        base = 3
    elif op.startswith("j"):
        base = 3 if taken else 1
    elif op == "ret":
        base = 5
    else:
        raise ValueError("Unmodeled opcode: " + op + " " + operand)
    if upper and mem:
        base += 1  # Pessimistically include AGI on every addressed operand.
        address = operand.split("[", 1)[1].split("]", 1)[0]
        if "*" in address or "+" in address:
            base += 1  # Indexed-address penalty assumption.
    return base

def estimate(packets, upper):
    choose = max if upper else min
    visiting = set()
    @functools.lru_cache(None)
    def walk(pc, iteration):
        key = pc, iteration
        if key in visiting:
            raise ValueError("Unexpected control-flow cycle; inspect listing")
        visiting.add(key)
        op, operand = instructions[pc]
        if op == "ret":
            value = cost(op, operand, upper)
        elif pc == loop_jump:
            value = cost(op, operand, upper, True) + walk(loop_head, iteration + 1)
        elif pc == loop_head + 1:
            done = iteration == packets
            value = cost(op, operand, upper, done) + walk(target(operand) if done else pc + 1, iteration)
        elif op == "jmp":
            value = cost(op, operand, upper, True) + walk(target(operand), iteration)
        elif op.startswith("j"):
            value = choose(cost(op, operand, upper, False) + walk(pc + 1, iteration),
                           cost(op, operand, upper, True) + walk(target(operand), iteration))
        else:
            value = cost(op, operand, upper) + walk(pc + 1, iteration)
        visiting.remove(key)
        return value
    return walk(0, 0)

rows = []
# The loop envelope is affine: each packet traverses the same bounded graph.
# Evaluate one/two iterations to avoid deep recursion at the 64-packet limit.
one = [estimate(1, upper) for upper in (False, True)]
two = [estimate(2, upper) for upper in (False, True)]
for packets in ((4,) if args.room else (4, 8, 12, 16, 20, 24, 64)):
    lower, upper = [one[i] + (packets-1)*(two[i]-one[i]) for i in (0, 1)]
    history_reads = 20 if args.room else 3 * packets
    scenarios = {str(extra): upper + history_reads * extra for extra in (0, 10, 18, 28)}
    row = dict(packets=packets, cache_hit_lower=lower, cache_hit_upper=upper,
               upper_with_extra_cycles_per_history_read=scenarios)
    rows.append(row)
    print(json.dumps(row))
report = dict(listing_sha256=hashlib.sha256(raw).hexdigest(),
              method=("bh_room_frame" if args.room else "bh_frame") + " bounded control-flow envelope; multiplier 13..26 clocks; conditional memory scenarios",
              excludes=["Body", "dither", "moving controls", "ADC/DAC analogue simulation", "DMA/IRQ and GUI", "DAC-byte packing and render-loop overhead", "code/patch/stack cache misses and write-buffer stalls"],
              clocks_per_frame={str(mhz): mhz * 1e6 / 21739 for mhz in (33, 66, 100)},
              rows=rows)
with open(args.json, "w", encoding="utf-8") as out:
    json.dump(report, out, indent=2)
