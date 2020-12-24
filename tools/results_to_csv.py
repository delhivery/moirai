#!/bin/python

import csv
import json

f = open('/home/amitprakash/moirai/build/outputs.json', 'r')
data = json.load(f)
f.close()

rows = []
rows.append(['Waybill', 'Bag', 'Location', 'Arrival', 'Inbound Edge Code', 'Inbound Edge Name'])

for record in data:
    waybill = record['package']
    bag = record['waybill']
    for location in record.get('earliest', {}).get('locations', []):
        loc = location.get('name')
        arr = location.get('arrival')
        iec = location.get('route', {}).get('code', '').split('.')[0]
        ien = location.get('route', {}).get('name')
        rows.append([waybill, bag, loc, arr, iec, ien])


f = open('/home/amitprakash/moirai/build/response.csv', 'w')
writer = csv.writer(f)
writer.writerows(rows)
f.close()
