import re

lines = []
while True:
	try:
		lines.append(input())
	except EOFError:
		break

deletions = []

for i, line in enumerate(lines):
	if line.startswith('VEX') or line.startswith('EVEX') or re.search(r'[x-z]mm', line):
		lines[i] = vex(line)
		continue
	if not (line[0].isdigit() or line[0] == 'R' or 0x41 <= ord(line[0]) <= 0x46):
		deletions.append(i)
	if re.match(r'REX \+ ', line):
		deletions.append(i)
	line = re.sub(r'REX\.W \+ ', 'w', line, count=100000)
	line = re.sub(r' (?=[0-9A-F]{2})', '', line, count=100000)
	line = re.sub(r'\/?(r|i|c)[bwdq] ', '', line, count=100000)
	line = re.sub(r'\+ r[bwdq]', '', line, count=100000)
	line = re.sub(r'[RMID]{1,} .+', '', line, count=100000)
	line = re.sub(r' (?=\/\d)', '', line, count=100000)
	line = line.lower()
	line = line.replace('eax', '0C')
	line = line.replace('rax', '0D')
	line = line.replace('al', '0A')
	line = line.replace('ax', '0B')
	line = line.replace('cl', '1A')
	line = line.replace(', 1', ', L1')
	line = line.replace('r8', 'RA')
	line = line.replace('r16', 'RB')
	line = line.replace('r32', 'RC')
	line = line.replace('r64', 'RD')
	line = line.replace('r/m8', 'MA')
	line = line.replace('r/m16', 'MB')
	line = line.replace('r/m32', 'MC')
	line = line.replace('r/m64', 'MD')
	line = line.replace('imm8', 'IA')
	line = line.replace('imm16', 'IB')
	line = line.replace('imm32', 'IC')
	line = line.replace('imm64', 'ID')
	line = line.replace('rel8', 'IA')
	line = line.replace('rel16', 'IB')
	line = line.replace('rel32', 'IC')
	line = line.replace('rel64', 'ID')
	line = line.replace(',', '')
	line = re.sub(r'(w?[0-9a-f/]+) (.+)', r'\2\1', line, count=100000)
	lines[i] = line

deletions.reverse()

for line in deletions:
	lines.pop(line)

for line in lines:
	print(line)
