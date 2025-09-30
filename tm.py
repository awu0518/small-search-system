import re
count = 0
eh = 0
pattern = re.compile(r'\b[A-Za-z](?:\.[A-Za-z]){1,2}\.?\b')

with open("collection.tsv", "r", encoding="utf-8", errors="ignore") as f:
    for line in f:
        line = line.strip()
        if pattern.search(line):   # search instead of match
            eh += 1
        count += 1
print(count)
print(eh)