@echo off
node test.create.table.js
node test.insert.values.js
node test.select.values.js

echo test find...
node test.$and.js
node test.$or.js
node test.$gt.js
node test.$in.js
node test.complex.find.js
node test.select.field.js
echo test insert 
node test.table.insert.js
echo test update
node test.table.update.js
echo test delete
node test.table.delete.js
pause