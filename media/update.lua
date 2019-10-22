countries={ 'ru', 'ua', 'kz', 'bg', 'tr', 'de' }

--[[ countries=
{
    af,    --Afghanistan
    al,    --Albania
    dz,    --Algeria
    ad,    --Andorra
    ao,    --Angola
    ar,    --Argentina
    am,    --Armenia
    aw,    --Aruba
    au,    --Australia
    at,    --Austria
    az,    --Azerbaijan
    bs,    --Bahamas
    bh,    --Bahrain
    bd,    --Bangladesh
    bb,    --Barbados
    by,    --Belarus
    be,    --Belgium
    bo,    --Bolivia
    ba,    --Bosnia and Herzegovina
    br,    --Brazil
    bn,    --Brunei
    bg,    --Bulgaria
    bf,    --Burkina Faso
    kh,    --Cambodia
    cm,    --Cameroon
    ca,    --Canada
    cv,    --Cape Verde
    cl,    --Chile
    cn,    --China
    co,    --Colombia
    cr,    --Costa Rica
    hr,    --Croatia
    cu,    --Cuba
    cw,    --Curaçao
    cy,    --Cyprus
    cz,    --Czech Republic
    cd,    --Democratic Republic of the Congo
    dk,    --Denmark
    do,    --Dominican Republic
    ec,    --Ecuador
    eg,    --Egypt
    sv,    --El Salvador
    gq,    --Equatorial Guinea
    ee,    --Estonia
    et,    --Ethiopia
    fo,    --Faroe Islands
    fi,    --Finland
    fj,    --Fiji
    fr,    --France
    gm,    --Gambia
    ge,    --Georgia
    de,    --Germany
    gh,    --Ghana
    gi,    --Gibraltar
    gr,    --Greece
    gd,    --Grenada
    gp,    --Guadeloupe
    gu,    --Guam
    gt,    --Guatemala
    gy,    --Guyana
    ht,    --Haiti
    hn,    --Honduras
    hk,    --Hong Kong
    hu,    --Hungary
    is,    --Iceland
    in,    --India
    id,    --Indonesia
    int,   --International
    ir,    --Iran
    iq,    --Iraq
    ie,    --Ireland
    il,    --Israel
    it,    --Italy
    ci,    --Ivory Coast
    jm,    --Jamaica
    jp,    --Japan
    jo,    --Jordan
    kz,    --Kazakhstan
    ke,    --Kenya
    xk,    --Kosovo
    kw,    --Kuwait
    kg,    --Kyrgyzstan
    la,    --Laos
    lv,    --Latvia
    lb,    --Lebanon
    ly,    --Libya
    li,    --Liechtenstein
    lt,    --Lithuania
    lu,    --Luxembourg
    mo,    --Macau
    mk,    --Macedonia
    mg,    --Madagascar
    my,    --Malaysia
    mv,    --Maldives
    mt,    --Malta
    mx,    --Mexico
    md,    --Moldova
    mc,    --Monaco
    mn,    --Mongolia
    me,    --Montenegro
    ma,    --Morocco
    mz,    --Mozambique
    mm,    --Myanmar
    np,    --Nepal
    nl,    --Netherlands
    nz,    --New Zealand
    ni,    --Nicaragua
    ne,    --Niger
    ng,    --Nigeria
    kp,    --North Korea
    no,    --Norway
    om,    --Oman
    pk,    --Pakistan
    ps,    --Palestine
    pa,    --Panama
    py,    --Paraguay
    pe,    --Peru
    ph,    --Philippines
    pl,    --Poland
    pt,    --Portugal
    pr,    --Puerto Rico
    qa,    --Qatar
    cg,    --Republic of the Congo
    ro,    --Romania
    ru,    --Russia
    rw,    --Rwanda
    kn,    --Saint Kitts and Nevis
    sm,    --San Marino
    sa,    --Saudi Arabia
    sn,    --Senegal
    rs,    --Serbia
    sl,    --Sierra Leone
    sg,    --Singapore
    sx,    --Sint Maarten
    sk,    --Slovakia
    si,    --Slovenia
    so,    --Somalia
    za,    --South Africa
    kr,    --South Korea
    es,    --Spain
    lk,    --Sri Lanka
    sd,    --Sudan
    sr,    --Suriname
    se,    --Sweden
    ch,    --Switzerland
    sy,    --Syria
    tw,    --Taiwan
    tj,    --Tajikistan
    tz,    --Tanzania
    th,    --Thailand
    tg,    --Togo
    tt,    --Trinidad and Tobago
    tn,    --Tunisia
    tr,    --Turkey
    tm,    --Turkmenistan
    ug,    --Uganda
    ua,    --Ukraine
    ae,    --United Arab Emirates
    uk,    --United Kingdom
    us,    --United States
    uy,    --Uruguay
    uz,    --Uzbekistan
    ve,    --Venezuela
    vn,    --Vietnam
    vi,    --Virgin Islands of the United States
    eh,    --Western Sahara
    ye,    --Yemen
    zw     --Zimbabwe
} ]]

os.execute('wget -O index.m3u https://raw.githubusercontent.com/iptv-org/iptv/master/index.m3u')

fd=io.open('index.m3u','rb')

s=fd:read('*a')

fd:close()

os.execute('rm -f index.m3u')

list={}

for name,country in string.gmatch(s,"#EXTINF:%-1,(.-)\r?\nchannels/(%a+)%.m3u\r?\n") do
    list[country]=name;

--    print('    '..country..',    --'..name)
end

for i,country in ipairs(countries) do
    name=list[country]

    if name then
        os.execute('wget -O "'..name..'.m3u" "https://raw.githubusercontent.com/iptv-org/iptv/master/channels/'..country..'.m3u"')
    end
end
