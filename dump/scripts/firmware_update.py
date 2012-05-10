#!/bin/python
import cgi, os
import cgitb

FIRMWARE_PATH = '/mnt/cf/storage/firmware'

cgitb.enable()

form = cgi.FieldStorage()

firmware_item = form['firmware']

# Test if the file was uploaded
if firmware_item.filename:
   filename = os.path.basename(firmware_item.filename)
   open(FIRMWARE_PATH + filename, 'wb').write(firmware_item.file.read())
   message = 'The file "' + fn + '" was uploaded successfully'
   
else:
   message = 'No file was uploaded'
   
print """\
Content-Type: text/html\n
<html><body>
<p>%s</p>
</body></html>
""" % (message,)