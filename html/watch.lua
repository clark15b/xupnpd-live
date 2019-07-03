status(200,"OK", { 'Content-Type: text/html' } )

p=playlist()[tonumber(args.s)]

print('<html>')
print('<head>')
print('<meta http-equiv="Content-type" content="text/html; charset=utf-8"/>')
print('<title>OTTBox - ' .. p.name .. '</title>')
print('</head>')
print('<body>')

print('<b><a href="/index.lua">[Назад]</a></b><br><br>')

for i,j in ipairs(p.channels) do
    local s=string.match(j.url,'/(%w+)%.ts')

    print(i.. '. <a href="/playlist.lua?s='..s..'">' .. j.name .. '</a><br>')
end

print('<br><b><a href="/index.lua">[Назад]</a></b>')

print('</body>')
print('</html>')