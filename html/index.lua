status(200,"OK", { 'Content-Type: text/html' } )

print('<html>')
print('<head>')
print('<meta http-equiv="Content-type" content="text/html; charset=utf-8"/>')
print('<title>OTTBox</title>')
print('</head>')
print('<body>')

print('<h1>' .. env.device_name .. '</h1>')

print('<a href=/playlist.lua>Загрузить M3U8 плейлист</a><br><br>')

print('<a href=/control.html>Просмотр на ТВ</a><br><br>')

p=playlist()

for i,j in ipairs(p) do
    print('<b><a href=/watch.lua?s=' .. i .. '>#' .. j.name .. '</a></b>')
end

print("<br><br><p style='font-size: 10pt' align=left>")
print('Copyright (C) 2018 Anton Burdinuk<br>')
print('clark15b@gmail.com')
print('</p>')

print('</body>')
print('</html>')