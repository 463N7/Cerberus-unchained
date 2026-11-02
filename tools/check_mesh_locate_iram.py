"""Inspect linked C5 Locate/parser callback placement; never attaches a device.

This is a native-image gate, not a host compilation or runtime ISR-stack proof.
The project parser closure is followed recursively. SDK/ROM entries reached by
project code must be internal; their complete vendor call graph is not claimed.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
from build_toolchain import resolve_build_tools

PROJECT = ('mesh_mac_parse', 'overlaps', 'take', 'little_value', 'read_pan',
           'read_address', 'pan_presence', 'read_security', 'read_header_ies',
           'locate_receive', 'locate_increment', 'locate_clock_fault')

CONTROL = {'j', 'jr', 'jal', 'jalr', 'call', 'tail', 'b', 'beq', 'bne', 'blt',
           'bge', 'bltu', 'bgeu', 'beqz', 'bnez', 'blez', 'bgez', 'bltz', 'bgtz',
           'bgt', 'ble', 'bgtu', 'bleu'}
# This gate deliberately rejects unreviewed ISA spellings instead of assuming
# they cannot transfer control. Objdump may print compressed opcodes either as
# their normal aliases or with the c. prefix; both forms use the same policy.
NONCONTROL = set('''add addi sub lui auipc li mv nop neg not and andi or ori xor
    xori sll slli srl srli sra srai slt slti sltu sltiu seqz snez sltz sgtz
    lb lbu lh lhu lw lwu sb sh sw mul mulh mulhu mulhsu div divu rem remu
    addi4spn addi16sp lwsp swsp andn orn xnor min max minu maxu clz ctz cpop
    rol ror rori rev8 orc.b sext.b sext.h zext.b zext.h zext.w
    sh1add sh2add sh3add bclr bclri bext bexti binv binvi bset bseti
    csrr csrw csrs csrc csrwi csrsi csrci csrrw csrrs csrrc csrrwi csrrsi csrrci
    fence fence.i rdcycle rdcycleh rdtime rdtimeh rdinstret rdinstreth'''.split())


def decode_instructions(text: str, start: int, size: int) -> list[dict]:
    """Require complete, ordered 16/32-bit RV32 instruction coverage of a symbol.

    This rejects data directives, unknown mnemonics, omitted/duplicate bytes and
    every unresolved control transfer. It does not follow vendor callees or
    resolve dynamic data accesses; those limits remain explicit in the receipt.
    """
    cursor = start
    decoded = []
    for line in text.splitlines():
        if not re.match(r'^\s*[0-9a-fA-F]+:', line):
            continue
        match = re.fullmatch(r'\s*([0-9a-fA-F]+):\s+([0-9a-fA-F]{4}|[0-9a-fA-F]{8})'
                             r'\s+([a-z][a-z0-9.]*)\s*(.*?)\s*', line)
        if not match:
            raise RuntimeError('Unrecognized native instruction line: ' + line)
        address, encoding, op, args = match.groups()
        address = int(address, 16)
        length = len(encoding) // 2
        if address != cursor or address + length > start + size:
            raise RuntimeError('Incomplete/overlapping native instruction coverage: ' + line)
        cursor += length
        normalized = op[2:] if op.startswith('c.') else op
        targets = [(int(a, 16), name) for a, name in
                   re.findall(r'(?:#\s*)?\b([0-9a-fA-F]{8})\s+<([^>]+)>', args)]
        # The only allowed unresolved indirect transfer is an exact return.
        compact_args = re.sub(r'\s+', '', args)
        returns = (normalized == 'ret' and not compact_args) or (
            normalized == 'jr' and compact_args in ('ra', 'x1')) or (
            normalized == 'jalr' and re.fullmatch(r'(?:zero|x0),(?:0\((?:ra|x1)\)|(?:ra|x1),0)', compact_args))
        control = normalized in CONTROL
        if not returns:
            if control:
                if len(targets) != 1:
                    raise RuntimeError('Unresolved/ambiguous native control transfer: ' + line)
            elif normalized not in NONCONTROL:
                raise RuntimeError('Unreviewed native instruction: ' + line)
        decoded.append({'address': address, 'op': op, 'args': args,
                        'control': control and not returns, 'targets': targets})
    if not decoded or cursor != start + size:
        raise RuntimeError('Native instruction coverage does not span the complete symbol')
    return decoded


def self_test() -> None:
    """Python-only regression vectors; no compiler, ELF or device is used."""
    checks = 0
    def good(text, size):
        nonlocal checks
        decoded = decode_instructions(text, 0x40800000, size)
        checks += 1
        return decoded
    def bad(text, size):
        nonlocal checks
        try:
            decode_instructions(text, 0x40800000, size)
        except RuntimeError:
            checks += 1
        else:
            raise AssertionError('Unsafe disassembly accepted: ' + text)
    good('40800000 <fn>:\n40800000: 1141 addi sp,sp,-16\n40800002: 00000517 auipc a0,0x0\n'
         '40800006: 8082 ret', 8)
    for op in sorted(CONTROL | {'c.j', 'c.jal', 'c.jr', 'c.jalr', 'c.beqz', 'c.bnez'}):
        bad(f'40800000: 0000006f {op} 40800010', 4)
        bad(f'40800000: 0000006f {op} t0', 4)
        instruction = good(f'40800000: 0000006f {op} 40800010 <callee>', 4)[0]
        assert instruction['control'] and instruction['targets'] == [(0x40800010, 'callee')]
        checks += 1
    for op, args in (('ret',''), ('jr','ra'), ('c.jr','ra'), ('jr','x1'),
                     ('jalr','zero,0(ra)'), ('jalr','x0,x1,0')):
        assert not good(f'40800000: 8082 {op} {args}', 2)[0]['control']
        checks += 1
    for line in ('40800000: 0000006f cm.jalt 12', '40800000: 0000006f mret',
                 '40800000: 0000 .short 0', '40800000: 00000000 unimp',
                 '40800000: deadbeef ???', '40800000: 00 00 nop'):
        bad(line, 4)
    bad('40800000: 1141 nop\n40800004: 8082 ret', 6)  # omitted bytes
    bad('40800000: 1141 nop\n40800000: 8082 ret', 4)  # duplicate
    bad('40800000: 1141 nop\n ...', 4)               # truncated output
    bad('40800000: 00000013 nop', 2)                 # overrun
    bad('40800000: 1141 nop\n40800002: 8082 ret', 2) # extra instruction
    bad('40800000 <fn>:', 4)                         # no instructions
    print('C5 Locate IRAM disassembly parser:', checks, 'Python-only checks PASS')


def verify(root: Path, image: Path | None = None, output: Path | None = None) -> dict:
    build = root / 'c5_firmware/.pio/build/c5-onboard'
    tools = resolve_build_tools(build, 'esp32c5')
    tool = Path(tools['objdump'])
    image = (image or build / 'firmware.elf').resolve(strict=True)
    output = output or root / 'artifacts/integration/c5-mesh-locate-iram.json'
    def run(*args):
        return subprocess.check_output([str(tool), *args, str(image)], text=True, timeout=60)
    symbols = []
    for line in run('-t').splitlines():
        p = line.split()
        if len(p) < 5 or not re.fullmatch('[0-9a-fA-F]+', p[0]) or not re.fullmatch('[0-9a-fA-F]+', p[-2]):
            continue
        symbols.append(dict(name=p[-1], address=int(p[0],16), size=int(p[-2],16), section=p[-3],
                            function='F' in p[1:-3]))
    def base(name):
        return name.split('.')[0]
    def internal(s):
        return s['section'].startswith(('.iram', '.dram')) or (
            s['section'] == '*ABS*' and 0x40000000 <= s['address'] < 0x40080000)
    def named(name):
        found = [s for s in symbols if s['name'] == name and s['function']]
        if len(found) != 1:
            raise RuntimeError('Missing/ambiguous required native function: ' + name)
        return found[0]
    required = [named('mesh_mac_parse'), named('locate_receive')]
    functions = list(required)
    queued = {s['address'] for s in functions}
    # These two small lookup arrays are otherwise flash rodata. Compiler removal
    # is permitted only when no object symbol remains (instructions then carry
    # the constants); any retained object must be internal DRAM.
    tables = [s for s in symbols if base(s['name']) in ('key_lengths','mic_lengths')]
    for s in tables:
        if not s['section'].startswith('.dram'):
            raise RuntimeError('Parser lookup table outside DRAM: ' + s['name'])
    inspected = []
    external = {}
    texts = []
    for s in functions:
        if not s['section'].startswith('.iram') or not s['size']:
            raise RuntimeError('Project callback/parser code outside IRAM: ' + s['name'])
        text = run('-d', '-j', s['section'], '--start-address='+hex(s['address']),
                   '--stop-address='+hex(s['address']+s['size']))
        texts.append(text)
        calls=[]; references=[]
        instructions=decode_instructions(text,s['address'],s['size'])
        for instruction in instructions:
            is_call=instruction['control']
            for addr,name in instruction['targets']:
                if s['address'] <= addr < s['address']+s['size']: continue
                candidates=[v for v in symbols if v['address']==addr or
                            (v['size'] and v['address']<=addr<v['address']+v['size'])]
                if is_call: candidates=[v for v in candidates if v['function']]
                chosen=[v for v in candidates if v['name']==name.split('+')[0]] or candidates
                if not chosen or not any(internal(v) for v in chosen):
                    raise RuntimeError('Non-internal/unresolved callback reference: '+s['name']+' -> '+name)
                target_symbol=next(v for v in chosen if internal(v))
                item={'address':hex(addr),'name':name,'section':target_symbol['section']}
                if is_call:
                    calls.append(item)
                    if base(target_symbol['name']) in PROJECT:
                        if target_symbol['address'] not in queued:
                            queued.add(target_symbol['address']); functions.append(target_symbol)
                    else: external[target_symbol['name']]=item
                else: references.append(item)
        inspected.append({**s,'address':hex(s['address']),'direct_calls':calls,'symbolic_references':references,
                          'instructions':len(instructions)})
    sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
    sources=['shared/mesh_mac.c','shared/mesh_mac.h','shared/mesh_locate_protocol.h',
             'c5_firmware/main/ieee802154_locate_impl.h','c5_firmware/main/ieee802154_scanner.c',
             'c5_firmware/main/CMakeLists.txt']
    result={'status':'passed','hardware_validated':False,'elf':str(image),'elf_sha256':sha(image),
            'checker_sha256':sha(Path(__file__)),'objdump':str(tool),'objdump_sha256':sha(tool),
            'source_hashes':{p:sha(root/p) for p in sources},'functions':inspected,'lookup_tables':tables,
            'required_roots':[s['name'] for s in required],'internal_sdk_rom_entries':list(external.values()),
            'limits':'Linked placement of project callback/parser and referenced lookup objects/direct SDK entries only. Vendor transitive call graph, dynamic unannotated data accesses, full ISR call-chain stack, physical interrupt rate and latency are not proven.'}
    output.parent.mkdir(parents=True,exist_ok=True)
    output.with_suffix('.txt').write_text('\n'.join(texts),encoding='utf-8')
    output.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print('C5 Locate/parser native IRAM gate passed:',len(inspected),'project functions;',len(tables),'lookup objects')
    return result


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1])
    parser.add_argument('--elf',type=Path)
    parser.add_argument('--output',type=Path)
    parser.add_argument('--self-test',action='store_true',help='Run only Python disassembly parser regressions')
    args=parser.parse_args()
    if args.self_test:
        self_test()
    else:
        verify(args.root.resolve(),args.elf,args.output)
