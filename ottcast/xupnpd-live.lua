-- Kineskop.tv (10min public)
function kineskop(url)
    return string.match(get(url),"loadStream%(decodeURIComponent%(getURLParam%('src','(.-)'%)")

--    return 'http://kineskop.tv/hls/211.m3u8?id=_STA543066&watch_time=300&watch_start=1468311638&p=3f162c1f7c1c05e0'
end

-- JurnalTV MD
function jurnaltv(url)
    return string.match(get(url),'"(http://live%.cdn%.jurnaltv%.md/.-)"')

--    http://live.cdn.jurnaltv.md/JurnalTV_HD/index.m3u8?token=dd24f213e31a66a3c22cc94059c27ff2bb11d08d&amp;expire=1525463029
end