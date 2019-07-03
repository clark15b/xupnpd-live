status(200,"OK", { 'Content-Type: audio/x-mpegurl', 'Content-Disposition: attachment; filename="playlist.m3u"' } )

print('#EXTM3U')

if not args.s then
    for i,j in pairs(playlist()[1].channels) do
        print('#EXTINF:0, '..j.name)
        print(j.url)
    end
else
    p=get_channel_by_id(args.s)

    if p then
        print('#EXTINF:0, '..p.name)
        print(p.url)
    end
end
