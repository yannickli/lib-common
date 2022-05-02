/***************************************************************************/
/*                                                                         */
/* Copyright 2022 INTERSEC SA                                              */
/*                                                                         */
/* Licensed under the Apache License, Version 2.0 (the "License");         */
/* you may not use this file except in compliance with the License.        */
/* You may obtain a copy of the License at                                 */
/*                                                                         */
/*     http://www.apache.org/licenses/LICENSE-2.0                          */
/*                                                                         */
/* Unless required by applicable law or agreed to in writing, software     */
/* distributed under the License is distributed on an "AS IS" BASIS,       */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.*/
/* See the License for the specific language governing permissions and     */
/* limitations under the License.                                          */
/*                                                                         */
/***************************************************************************/

const uint64_t crc64table[4][256] = {
    {
        LE64_T(0x0000000000000000), LE64_T(0xB32E4CBE03A75F6F),
        LE64_T(0xF4843657A840A05B), LE64_T(0x47AA7AE9ABE7FF34),
        LE64_T(0x7BD0C384FF8F5E33), LE64_T(0xC8FE8F3AFC28015C),
        LE64_T(0x8F54F5D357CFFE68), LE64_T(0x3C7AB96D5468A107),
        LE64_T(0xF7A18709FF1EBC66), LE64_T(0x448FCBB7FCB9E309),
        LE64_T(0x0325B15E575E1C3D), LE64_T(0xB00BFDE054F94352),
        LE64_T(0x8C71448D0091E255), LE64_T(0x3F5F08330336BD3A),
        LE64_T(0x78F572DAA8D1420E), LE64_T(0xCBDB3E64AB761D61),
        LE64_T(0x7D9BA13851336649), LE64_T(0xCEB5ED8652943926),
        LE64_T(0x891F976FF973C612), LE64_T(0x3A31DBD1FAD4997D),
        LE64_T(0x064B62BCAEBC387A), LE64_T(0xB5652E02AD1B6715),
        LE64_T(0xF2CF54EB06FC9821), LE64_T(0x41E11855055BC74E),
        LE64_T(0x8A3A2631AE2DDA2F), LE64_T(0x39146A8FAD8A8540),
        LE64_T(0x7EBE1066066D7A74), LE64_T(0xCD905CD805CA251B),
        LE64_T(0xF1EAE5B551A2841C), LE64_T(0x42C4A90B5205DB73),
        LE64_T(0x056ED3E2F9E22447), LE64_T(0xB6409F5CFA457B28),
        LE64_T(0xFB374270A266CC92), LE64_T(0x48190ECEA1C193FD),
        LE64_T(0x0FB374270A266CC9), LE64_T(0xBC9D3899098133A6),
        LE64_T(0x80E781F45DE992A1), LE64_T(0x33C9CD4A5E4ECDCE),
        LE64_T(0x7463B7A3F5A932FA), LE64_T(0xC74DFB1DF60E6D95),
        LE64_T(0x0C96C5795D7870F4), LE64_T(0xBFB889C75EDF2F9B),
        LE64_T(0xF812F32EF538D0AF), LE64_T(0x4B3CBF90F69F8FC0),
        LE64_T(0x774606FDA2F72EC7), LE64_T(0xC4684A43A15071A8),
        LE64_T(0x83C230AA0AB78E9C), LE64_T(0x30EC7C140910D1F3),
        LE64_T(0x86ACE348F355AADB), LE64_T(0x3582AFF6F0F2F5B4),
        LE64_T(0x7228D51F5B150A80), LE64_T(0xC10699A158B255EF),
        LE64_T(0xFD7C20CC0CDAF4E8), LE64_T(0x4E526C720F7DAB87),
        LE64_T(0x09F8169BA49A54B3), LE64_T(0xBAD65A25A73D0BDC),
        LE64_T(0x710D64410C4B16BD), LE64_T(0xC22328FF0FEC49D2),
        LE64_T(0x85895216A40BB6E6), LE64_T(0x36A71EA8A7ACE989),
        LE64_T(0x0ADDA7C5F3C4488E), LE64_T(0xB9F3EB7BF06317E1),
        LE64_T(0xFE5991925B84E8D5), LE64_T(0x4D77DD2C5823B7BA),
        LE64_T(0x64B62BCAEBC387A1), LE64_T(0xD7986774E864D8CE),
        LE64_T(0x90321D9D438327FA), LE64_T(0x231C512340247895),
        LE64_T(0x1F66E84E144CD992), LE64_T(0xAC48A4F017EB86FD),
        LE64_T(0xEBE2DE19BC0C79C9), LE64_T(0x58CC92A7BFAB26A6),
        LE64_T(0x9317ACC314DD3BC7), LE64_T(0x2039E07D177A64A8),
        LE64_T(0x67939A94BC9D9B9C), LE64_T(0xD4BDD62ABF3AC4F3),
        LE64_T(0xE8C76F47EB5265F4), LE64_T(0x5BE923F9E8F53A9B),
        LE64_T(0x1C4359104312C5AF), LE64_T(0xAF6D15AE40B59AC0),
        LE64_T(0x192D8AF2BAF0E1E8), LE64_T(0xAA03C64CB957BE87),
        LE64_T(0xEDA9BCA512B041B3), LE64_T(0x5E87F01B11171EDC),
        LE64_T(0x62FD4976457FBFDB), LE64_T(0xD1D305C846D8E0B4),
        LE64_T(0x96797F21ED3F1F80), LE64_T(0x2557339FEE9840EF),
        LE64_T(0xEE8C0DFB45EE5D8E), LE64_T(0x5DA24145464902E1),
        LE64_T(0x1A083BACEDAEFDD5), LE64_T(0xA9267712EE09A2BA),
        LE64_T(0x955CCE7FBA6103BD), LE64_T(0x267282C1B9C65CD2),
        LE64_T(0x61D8F8281221A3E6), LE64_T(0xD2F6B4961186FC89),
        LE64_T(0x9F8169BA49A54B33), LE64_T(0x2CAF25044A02145C),
        LE64_T(0x6B055FEDE1E5EB68), LE64_T(0xD82B1353E242B407),
        LE64_T(0xE451AA3EB62A1500), LE64_T(0x577FE680B58D4A6F),
        LE64_T(0x10D59C691E6AB55B), LE64_T(0xA3FBD0D71DCDEA34),
        LE64_T(0x6820EEB3B6BBF755), LE64_T(0xDB0EA20DB51CA83A),
        LE64_T(0x9CA4D8E41EFB570E), LE64_T(0x2F8A945A1D5C0861),
        LE64_T(0x13F02D374934A966), LE64_T(0xA0DE61894A93F609),
        LE64_T(0xE7741B60E174093D), LE64_T(0x545A57DEE2D35652),
        LE64_T(0xE21AC88218962D7A), LE64_T(0x5134843C1B317215),
        LE64_T(0x169EFED5B0D68D21), LE64_T(0xA5B0B26BB371D24E),
        LE64_T(0x99CA0B06E7197349), LE64_T(0x2AE447B8E4BE2C26),
        LE64_T(0x6D4E3D514F59D312), LE64_T(0xDE6071EF4CFE8C7D),
        LE64_T(0x15BB4F8BE788911C), LE64_T(0xA6950335E42FCE73),
        LE64_T(0xE13F79DC4FC83147), LE64_T(0x521135624C6F6E28),
        LE64_T(0x6E6B8C0F1807CF2F), LE64_T(0xDD45C0B11BA09040),
        LE64_T(0x9AEFBA58B0476F74), LE64_T(0x29C1F6E6B3E0301B),
        LE64_T(0xC96C5795D7870F42), LE64_T(0x7A421B2BD420502D),
        LE64_T(0x3DE861C27FC7AF19), LE64_T(0x8EC62D7C7C60F076),
        LE64_T(0xB2BC941128085171), LE64_T(0x0192D8AF2BAF0E1E),
        LE64_T(0x4638A2468048F12A), LE64_T(0xF516EEF883EFAE45),
        LE64_T(0x3ECDD09C2899B324), LE64_T(0x8DE39C222B3EEC4B),
        LE64_T(0xCA49E6CB80D9137F), LE64_T(0x7967AA75837E4C10),
        LE64_T(0x451D1318D716ED17), LE64_T(0xF6335FA6D4B1B278),
        LE64_T(0xB199254F7F564D4C), LE64_T(0x02B769F17CF11223),
        LE64_T(0xB4F7F6AD86B4690B), LE64_T(0x07D9BA1385133664),
        LE64_T(0x4073C0FA2EF4C950), LE64_T(0xF35D8C442D53963F),
        LE64_T(0xCF273529793B3738), LE64_T(0x7C0979977A9C6857),
        LE64_T(0x3BA3037ED17B9763), LE64_T(0x888D4FC0D2DCC80C),
        LE64_T(0x435671A479AAD56D), LE64_T(0xF0783D1A7A0D8A02),
        LE64_T(0xB7D247F3D1EA7536), LE64_T(0x04FC0B4DD24D2A59),
        LE64_T(0x3886B22086258B5E), LE64_T(0x8BA8FE9E8582D431),
        LE64_T(0xCC0284772E652B05), LE64_T(0x7F2CC8C92DC2746A),
        LE64_T(0x325B15E575E1C3D0), LE64_T(0x8175595B76469CBF),
        LE64_T(0xC6DF23B2DDA1638B), LE64_T(0x75F16F0CDE063CE4),
        LE64_T(0x498BD6618A6E9DE3), LE64_T(0xFAA59ADF89C9C28C),
        LE64_T(0xBD0FE036222E3DB8), LE64_T(0x0E21AC88218962D7),
        LE64_T(0xC5FA92EC8AFF7FB6), LE64_T(0x76D4DE52895820D9),
        LE64_T(0x317EA4BB22BFDFED), LE64_T(0x8250E80521188082),
        LE64_T(0xBE2A516875702185), LE64_T(0x0D041DD676D77EEA),
        LE64_T(0x4AAE673FDD3081DE), LE64_T(0xF9802B81DE97DEB1),
        LE64_T(0x4FC0B4DD24D2A599), LE64_T(0xFCEEF8632775FAF6),
        LE64_T(0xBB44828A8C9205C2), LE64_T(0x086ACE348F355AAD),
        LE64_T(0x34107759DB5DFBAA), LE64_T(0x873E3BE7D8FAA4C5),
        LE64_T(0xC094410E731D5BF1), LE64_T(0x73BA0DB070BA049E),
        LE64_T(0xB86133D4DBCC19FF), LE64_T(0x0B4F7F6AD86B4690),
        LE64_T(0x4CE50583738CB9A4), LE64_T(0xFFCB493D702BE6CB),
        LE64_T(0xC3B1F050244347CC), LE64_T(0x709FBCEE27E418A3),
        LE64_T(0x3735C6078C03E797), LE64_T(0x841B8AB98FA4B8F8),
        LE64_T(0xADDA7C5F3C4488E3), LE64_T(0x1EF430E13FE3D78C),
        LE64_T(0x595E4A08940428B8), LE64_T(0xEA7006B697A377D7),
        LE64_T(0xD60ABFDBC3CBD6D0), LE64_T(0x6524F365C06C89BF),
        LE64_T(0x228E898C6B8B768B), LE64_T(0x91A0C532682C29E4),
        LE64_T(0x5A7BFB56C35A3485), LE64_T(0xE955B7E8C0FD6BEA),
        LE64_T(0xAEFFCD016B1A94DE), LE64_T(0x1DD181BF68BDCBB1),
        LE64_T(0x21AB38D23CD56AB6), LE64_T(0x9285746C3F7235D9),
        LE64_T(0xD52F0E859495CAED), LE64_T(0x6601423B97329582),
        LE64_T(0xD041DD676D77EEAA), LE64_T(0x636F91D96ED0B1C5),
        LE64_T(0x24C5EB30C5374EF1), LE64_T(0x97EBA78EC690119E),
        LE64_T(0xAB911EE392F8B099), LE64_T(0x18BF525D915FEFF6),
        LE64_T(0x5F1528B43AB810C2), LE64_T(0xEC3B640A391F4FAD),
        LE64_T(0x27E05A6E926952CC), LE64_T(0x94CE16D091CE0DA3),
        LE64_T(0xD3646C393A29F297), LE64_T(0x604A2087398EADF8),
        LE64_T(0x5C3099EA6DE60CFF), LE64_T(0xEF1ED5546E415390),
        LE64_T(0xA8B4AFBDC5A6ACA4), LE64_T(0x1B9AE303C601F3CB),
        LE64_T(0x56ED3E2F9E224471), LE64_T(0xE5C372919D851B1E),
        LE64_T(0xA26908783662E42A), LE64_T(0x114744C635C5BB45),
        LE64_T(0x2D3DFDAB61AD1A42), LE64_T(0x9E13B115620A452D),
        LE64_T(0xD9B9CBFCC9EDBA19), LE64_T(0x6A978742CA4AE576),
        LE64_T(0xA14CB926613CF817), LE64_T(0x1262F598629BA778),
        LE64_T(0x55C88F71C97C584C), LE64_T(0xE6E6C3CFCADB0723),
        LE64_T(0xDA9C7AA29EB3A624), LE64_T(0x69B2361C9D14F94B),
        LE64_T(0x2E184CF536F3067F), LE64_T(0x9D36004B35545910),
        LE64_T(0x2B769F17CF112238), LE64_T(0x9858D3A9CCB67D57),
        LE64_T(0xDFF2A94067518263), LE64_T(0x6CDCE5FE64F6DD0C),
        LE64_T(0x50A65C93309E7C0B), LE64_T(0xE388102D33392364),
        LE64_T(0xA4226AC498DEDC50), LE64_T(0x170C267A9B79833F),
        LE64_T(0xDCD7181E300F9E5E), LE64_T(0x6FF954A033A8C131),
        LE64_T(0x28532E49984F3E05), LE64_T(0x9B7D62F79BE8616A),
        LE64_T(0xA707DB9ACF80C06D), LE64_T(0x14299724CC279F02),
        LE64_T(0x5383EDCD67C06036), LE64_T(0xE0ADA17364673F59),
    }, {
        LE64_T(0x0000000000000000), LE64_T(0x54E979925CD0F10D),
        LE64_T(0xA9D2F324B9A1E21A), LE64_T(0xFD3B8AB6E5711317),
        LE64_T(0xC17D4962DC4DDAB1), LE64_T(0x959430F0809D2BBC),
        LE64_T(0x68AFBA4665EC38AB), LE64_T(0x3C46C3D4393CC9A6),
        LE64_T(0x10223DEE1795ABE7), LE64_T(0x44CB447C4B455AEA),
        LE64_T(0xB9F0CECAAE3449FD), LE64_T(0xED19B758F2E4B8F0),
        LE64_T(0xD15F748CCBD87156), LE64_T(0x85B60D1E9708805B),
        LE64_T(0x788D87A87279934C), LE64_T(0x2C64FE3A2EA96241),
        LE64_T(0x20447BDC2F2B57CE), LE64_T(0x74AD024E73FBA6C3),
        LE64_T(0x899688F8968AB5D4), LE64_T(0xDD7FF16ACA5A44D9),
        LE64_T(0xE13932BEF3668D7F), LE64_T(0xB5D04B2CAFB67C72),
        LE64_T(0x48EBC19A4AC76F65), LE64_T(0x1C02B80816179E68),
        LE64_T(0x3066463238BEFC29), LE64_T(0x648F3FA0646E0D24),
        LE64_T(0x99B4B516811F1E33), LE64_T(0xCD5DCC84DDCFEF3E),
        LE64_T(0xF11B0F50E4F32698), LE64_T(0xA5F276C2B823D795),
        LE64_T(0x58C9FC745D52C482), LE64_T(0x0C2085E60182358F),
        LE64_T(0x4088F7B85E56AF9C), LE64_T(0x14618E2A02865E91),
        LE64_T(0xE95A049CE7F74D86), LE64_T(0xBDB37D0EBB27BC8B),
        LE64_T(0x81F5BEDA821B752D), LE64_T(0xD51CC748DECB8420),
        LE64_T(0x28274DFE3BBA9737), LE64_T(0x7CCE346C676A663A),
        LE64_T(0x50AACA5649C3047B), LE64_T(0x0443B3C41513F576),
        LE64_T(0xF9783972F062E661), LE64_T(0xAD9140E0ACB2176C),
        LE64_T(0x91D78334958EDECA), LE64_T(0xC53EFAA6C95E2FC7),
        LE64_T(0x380570102C2F3CD0), LE64_T(0x6CEC098270FFCDDD),
        LE64_T(0x60CC8C64717DF852), LE64_T(0x3425F5F62DAD095F),
        LE64_T(0xC91E7F40C8DC1A48), LE64_T(0x9DF706D2940CEB45),
        LE64_T(0xA1B1C506AD3022E3), LE64_T(0xF558BC94F1E0D3EE),
        LE64_T(0x086336221491C0F9), LE64_T(0x5C8A4FB0484131F4),
        LE64_T(0x70EEB18A66E853B5), LE64_T(0x2407C8183A38A2B8),
        LE64_T(0xD93C42AEDF49B1AF), LE64_T(0x8DD53B3C839940A2),
        LE64_T(0xB193F8E8BAA58904), LE64_T(0xE57A817AE6757809),
        LE64_T(0x18410BCC03046B1E), LE64_T(0x4CA8725E5FD49A13),
        LE64_T(0x8111EF70BCAD5F38), LE64_T(0xD5F896E2E07DAE35),
        LE64_T(0x28C31C54050CBD22), LE64_T(0x7C2A65C659DC4C2F),
        LE64_T(0x406CA61260E08589), LE64_T(0x1485DF803C307484),
        LE64_T(0xE9BE5536D9416793), LE64_T(0xBD572CA48591969E),
        LE64_T(0x9133D29EAB38F4DF), LE64_T(0xC5DAAB0CF7E805D2),
        LE64_T(0x38E121BA129916C5), LE64_T(0x6C0858284E49E7C8),
        LE64_T(0x504E9BFC77752E6E), LE64_T(0x04A7E26E2BA5DF63),
        LE64_T(0xF99C68D8CED4CC74), LE64_T(0xAD75114A92043D79),
        LE64_T(0xA15594AC938608F6), LE64_T(0xF5BCED3ECF56F9FB),
        LE64_T(0x088767882A27EAEC), LE64_T(0x5C6E1E1A76F71BE1),
        LE64_T(0x6028DDCE4FCBD247), LE64_T(0x34C1A45C131B234A),
        LE64_T(0xC9FA2EEAF66A305D), LE64_T(0x9D135778AABAC150),
        LE64_T(0xB177A9428413A311), LE64_T(0xE59ED0D0D8C3521C),
        LE64_T(0x18A55A663DB2410B), LE64_T(0x4C4C23F46162B006),
        LE64_T(0x700AE020585E79A0), LE64_T(0x24E399B2048E88AD),
        LE64_T(0xD9D81304E1FF9BBA), LE64_T(0x8D316A96BD2F6AB7),
        LE64_T(0xC19918C8E2FBF0A4), LE64_T(0x9570615ABE2B01A9),
        LE64_T(0x684BEBEC5B5A12BE), LE64_T(0x3CA2927E078AE3B3),
        LE64_T(0x00E451AA3EB62A15), LE64_T(0x540D28386266DB18),
        LE64_T(0xA936A28E8717C80F), LE64_T(0xFDDFDB1CDBC73902),
        LE64_T(0xD1BB2526F56E5B43), LE64_T(0x85525CB4A9BEAA4E),
        LE64_T(0x7869D6024CCFB959), LE64_T(0x2C80AF90101F4854),
        LE64_T(0x10C66C44292381F2), LE64_T(0x442F15D675F370FF),
        LE64_T(0xB9149F60908263E8), LE64_T(0xEDFDE6F2CC5292E5),
        LE64_T(0xE1DD6314CDD0A76A), LE64_T(0xB5341A8691005667),
        LE64_T(0x480F903074714570), LE64_T(0x1CE6E9A228A1B47D),
        LE64_T(0x20A02A76119D7DDB), LE64_T(0x744953E44D4D8CD6),
        LE64_T(0x8972D952A83C9FC1), LE64_T(0xDD9BA0C0F4EC6ECC),
        LE64_T(0xF1FF5EFADA450C8D), LE64_T(0xA51627688695FD80),
        LE64_T(0x582DADDE63E4EE97), LE64_T(0x0CC4D44C3F341F9A),
        LE64_T(0x308217980608D63C), LE64_T(0x646B6E0A5AD82731),
        LE64_T(0x9950E4BCBFA93426), LE64_T(0xCDB99D2EE379C52B),
        LE64_T(0x90FB71CAD654A0F5), LE64_T(0xC41208588A8451F8),
        LE64_T(0x392982EE6FF542EF), LE64_T(0x6DC0FB7C3325B3E2),
        LE64_T(0x518638A80A197A44), LE64_T(0x056F413A56C98B49),
        LE64_T(0xF854CB8CB3B8985E), LE64_T(0xACBDB21EEF686953),
        LE64_T(0x80D94C24C1C10B12), LE64_T(0xD43035B69D11FA1F),
        LE64_T(0x290BBF007860E908), LE64_T(0x7DE2C69224B01805),
        LE64_T(0x41A405461D8CD1A3), LE64_T(0x154D7CD4415C20AE),
        LE64_T(0xE876F662A42D33B9), LE64_T(0xBC9F8FF0F8FDC2B4),
        LE64_T(0xB0BF0A16F97FF73B), LE64_T(0xE4567384A5AF0636),
        LE64_T(0x196DF93240DE1521), LE64_T(0x4D8480A01C0EE42C),
        LE64_T(0x71C2437425322D8A), LE64_T(0x252B3AE679E2DC87),
        LE64_T(0xD810B0509C93CF90), LE64_T(0x8CF9C9C2C0433E9D),
        LE64_T(0xA09D37F8EEEA5CDC), LE64_T(0xF4744E6AB23AADD1),
        LE64_T(0x094FC4DC574BBEC6), LE64_T(0x5DA6BD4E0B9B4FCB),
        LE64_T(0x61E07E9A32A7866D), LE64_T(0x350907086E777760),
        LE64_T(0xC8328DBE8B066477), LE64_T(0x9CDBF42CD7D6957A),
        LE64_T(0xD073867288020F69), LE64_T(0x849AFFE0D4D2FE64),
        LE64_T(0x79A1755631A3ED73), LE64_T(0x2D480CC46D731C7E),
        LE64_T(0x110ECF10544FD5D8), LE64_T(0x45E7B682089F24D5),
        LE64_T(0xB8DC3C34EDEE37C2), LE64_T(0xEC3545A6B13EC6CF),
        LE64_T(0xC051BB9C9F97A48E), LE64_T(0x94B8C20EC3475583),
        LE64_T(0x698348B826364694), LE64_T(0x3D6A312A7AE6B799),
        LE64_T(0x012CF2FE43DA7E3F), LE64_T(0x55C58B6C1F0A8F32),
        LE64_T(0xA8FE01DAFA7B9C25), LE64_T(0xFC177848A6AB6D28),
        LE64_T(0xF037FDAEA72958A7), LE64_T(0xA4DE843CFBF9A9AA),
        LE64_T(0x59E50E8A1E88BABD), LE64_T(0x0D0C771842584BB0),
        LE64_T(0x314AB4CC7B648216), LE64_T(0x65A3CD5E27B4731B),
        LE64_T(0x989847E8C2C5600C), LE64_T(0xCC713E7A9E159101),
        LE64_T(0xE015C040B0BCF340), LE64_T(0xB4FCB9D2EC6C024D),
        LE64_T(0x49C73364091D115A), LE64_T(0x1D2E4AF655CDE057),
        LE64_T(0x216889226CF129F1), LE64_T(0x7581F0B03021D8FC),
        LE64_T(0x88BA7A06D550CBEB), LE64_T(0xDC53039489803AE6),
        LE64_T(0x11EA9EBA6AF9FFCD), LE64_T(0x4503E72836290EC0),
        LE64_T(0xB8386D9ED3581DD7), LE64_T(0xECD1140C8F88ECDA),
        LE64_T(0xD097D7D8B6B4257C), LE64_T(0x847EAE4AEA64D471),
        LE64_T(0x794524FC0F15C766), LE64_T(0x2DAC5D6E53C5366B),
        LE64_T(0x01C8A3547D6C542A), LE64_T(0x5521DAC621BCA527),
        LE64_T(0xA81A5070C4CDB630), LE64_T(0xFCF329E2981D473D),
        LE64_T(0xC0B5EA36A1218E9B), LE64_T(0x945C93A4FDF17F96),
        LE64_T(0x6967191218806C81), LE64_T(0x3D8E608044509D8C),
        LE64_T(0x31AEE56645D2A803), LE64_T(0x65479CF41902590E),
        LE64_T(0x987C1642FC734A19), LE64_T(0xCC956FD0A0A3BB14),
        LE64_T(0xF0D3AC04999F72B2), LE64_T(0xA43AD596C54F83BF),
        LE64_T(0x59015F20203E90A8), LE64_T(0x0DE826B27CEE61A5),
        LE64_T(0x218CD888524703E4), LE64_T(0x7565A11A0E97F2E9),
        LE64_T(0x885E2BACEBE6E1FE), LE64_T(0xDCB7523EB73610F3),
        LE64_T(0xE0F191EA8E0AD955), LE64_T(0xB418E878D2DA2858),
        LE64_T(0x492362CE37AB3B4F), LE64_T(0x1DCA1B5C6B7BCA42),
        LE64_T(0x5162690234AF5051), LE64_T(0x058B1090687FA15C),
        LE64_T(0xF8B09A268D0EB24B), LE64_T(0xAC59E3B4D1DE4346),
        LE64_T(0x901F2060E8E28AE0), LE64_T(0xC4F659F2B4327BED),
        LE64_T(0x39CDD344514368FA), LE64_T(0x6D24AAD60D9399F7),
        LE64_T(0x414054EC233AFBB6), LE64_T(0x15A92D7E7FEA0ABB),
        LE64_T(0xE892A7C89A9B19AC), LE64_T(0xBC7BDE5AC64BE8A1),
        LE64_T(0x803D1D8EFF772107), LE64_T(0xD4D4641CA3A7D00A),
        LE64_T(0x29EFEEAA46D6C31D), LE64_T(0x7D0697381A063210),
        LE64_T(0x712612DE1B84079F), LE64_T(0x25CF6B4C4754F692),
        LE64_T(0xD8F4E1FAA225E585), LE64_T(0x8C1D9868FEF51488),
        LE64_T(0xB05B5BBCC7C9DD2E), LE64_T(0xE4B2222E9B192C23),
        LE64_T(0x1989A8987E683F34), LE64_T(0x4D60D10A22B8CE39),
        LE64_T(0x61042F300C11AC78), LE64_T(0x35ED56A250C15D75),
        LE64_T(0xC8D6DC14B5B04E62), LE64_T(0x9C3FA586E960BF6F),
        LE64_T(0xA0796652D05C76C9), LE64_T(0xF4901FC08C8C87C4),
        LE64_T(0x09AB957669FD94D3), LE64_T(0x5D42ECE4352D65DE),
    }, {
        LE64_T(0x0000000000000000), LE64_T(0x3F0BE14A916A6DCB),
        LE64_T(0x7E17C29522D4DB96), LE64_T(0x411C23DFB3BEB65D),
        LE64_T(0xFC2F852A45A9B72C), LE64_T(0xC3246460D4C3DAE7),
        LE64_T(0x823847BF677D6CBA), LE64_T(0xBD33A6F5F6170171),
        LE64_T(0x6A87A57F245D70DD), LE64_T(0x558C4435B5371D16),
        LE64_T(0x149067EA0689AB4B), LE64_T(0x2B9B86A097E3C680),
        LE64_T(0x96A8205561F4C7F1), LE64_T(0xA9A3C11FF09EAA3A),
        LE64_T(0xE8BFE2C043201C67), LE64_T(0xD7B4038AD24A71AC),
        LE64_T(0xD50F4AFE48BAE1BA), LE64_T(0xEA04ABB4D9D08C71),
        LE64_T(0xAB18886B6A6E3A2C), LE64_T(0x94136921FB0457E7),
        LE64_T(0x2920CFD40D135696), LE64_T(0x162B2E9E9C793B5D),
        LE64_T(0x57370D412FC78D00), LE64_T(0x683CEC0BBEADE0CB),
        LE64_T(0xBF88EF816CE79167), LE64_T(0x80830ECBFD8DFCAC),
        LE64_T(0xC19F2D144E334AF1), LE64_T(0xFE94CC5EDF59273A),
        LE64_T(0x43A76AAB294E264B), LE64_T(0x7CAC8BE1B8244B80),
        LE64_T(0x3DB0A83E0B9AFDDD), LE64_T(0x02BB49749AF09016),
        LE64_T(0x38C63AD73E7BDDF1), LE64_T(0x07CDDB9DAF11B03A),
        LE64_T(0x46D1F8421CAF0667), LE64_T(0x79DA19088DC56BAC),
        LE64_T(0xC4E9BFFD7BD26ADD), LE64_T(0xFBE25EB7EAB80716),
        LE64_T(0xBAFE7D685906B14B), LE64_T(0x85F59C22C86CDC80),
        LE64_T(0x52419FA81A26AD2C), LE64_T(0x6D4A7EE28B4CC0E7),
        LE64_T(0x2C565D3D38F276BA), LE64_T(0x135DBC77A9981B71),
        LE64_T(0xAE6E1A825F8F1A00), LE64_T(0x9165FBC8CEE577CB),
        LE64_T(0xD079D8177D5BC196), LE64_T(0xEF72395DEC31AC5D),
        LE64_T(0xEDC9702976C13C4B), LE64_T(0xD2C29163E7AB5180),
        LE64_T(0x93DEB2BC5415E7DD), LE64_T(0xACD553F6C57F8A16),
        LE64_T(0x11E6F50333688B67), LE64_T(0x2EED1449A202E6AC),
        LE64_T(0x6FF1379611BC50F1), LE64_T(0x50FAD6DC80D63D3A),
        LE64_T(0x874ED556529C4C96), LE64_T(0xB845341CC3F6215D),
        LE64_T(0xF95917C370489700), LE64_T(0xC652F689E122FACB),
        LE64_T(0x7B61507C1735FBBA), LE64_T(0x446AB136865F9671),
        LE64_T(0x057692E935E1202C), LE64_T(0x3A7D73A3A48B4DE7),
        LE64_T(0x718C75AE7CF7BBE2), LE64_T(0x4E8794E4ED9DD629),
        LE64_T(0x0F9BB73B5E236074), LE64_T(0x30905671CF490DBF),
        LE64_T(0x8DA3F084395E0CCE), LE64_T(0xB2A811CEA8346105),
        LE64_T(0xF3B432111B8AD758), LE64_T(0xCCBFD35B8AE0BA93),
        LE64_T(0x1B0BD0D158AACB3F), LE64_T(0x2400319BC9C0A6F4),
        LE64_T(0x651C12447A7E10A9), LE64_T(0x5A17F30EEB147D62),
        LE64_T(0xE72455FB1D037C13), LE64_T(0xD82FB4B18C6911D8),
        LE64_T(0x9933976E3FD7A785), LE64_T(0xA6387624AEBDCA4E),
        LE64_T(0xA4833F50344D5A58), LE64_T(0x9B88DE1AA5273793),
        LE64_T(0xDA94FDC5169981CE), LE64_T(0xE59F1C8F87F3EC05),
        LE64_T(0x58ACBA7A71E4ED74), LE64_T(0x67A75B30E08E80BF),
        LE64_T(0x26BB78EF533036E2), LE64_T(0x19B099A5C25A5B29),
        LE64_T(0xCE049A2F10102A85), LE64_T(0xF10F7B65817A474E),
        LE64_T(0xB01358BA32C4F113), LE64_T(0x8F18B9F0A3AE9CD8),
        LE64_T(0x322B1F0555B99DA9), LE64_T(0x0D20FE4FC4D3F062),
        LE64_T(0x4C3CDD90776D463F), LE64_T(0x73373CDAE6072BF4),
        LE64_T(0x494A4F79428C6613), LE64_T(0x7641AE33D3E60BD8),
        LE64_T(0x375D8DEC6058BD85), LE64_T(0x08566CA6F132D04E),
        LE64_T(0xB565CA530725D13F), LE64_T(0x8A6E2B19964FBCF4),
        LE64_T(0xCB7208C625F10AA9), LE64_T(0xF479E98CB49B6762),
        LE64_T(0x23CDEA0666D116CE), LE64_T(0x1CC60B4CF7BB7B05),
        LE64_T(0x5DDA28934405CD58), LE64_T(0x62D1C9D9D56FA093),
        LE64_T(0xDFE26F2C2378A1E2), LE64_T(0xE0E98E66B212CC29),
        LE64_T(0xA1F5ADB901AC7A74), LE64_T(0x9EFE4CF390C617BF),
        LE64_T(0x9C4505870A3687A9), LE64_T(0xA34EE4CD9B5CEA62),
        LE64_T(0xE252C71228E25C3F), LE64_T(0xDD592658B98831F4),
        LE64_T(0x606A80AD4F9F3085), LE64_T(0x5F6161E7DEF55D4E),
        LE64_T(0x1E7D42386D4BEB13), LE64_T(0x2176A372FC2186D8),
        LE64_T(0xF6C2A0F82E6BF774), LE64_T(0xC9C941B2BF019ABF),
        LE64_T(0x88D5626D0CBF2CE2), LE64_T(0xB7DE83279DD54129),
        LE64_T(0x0AED25D26BC24058), LE64_T(0x35E6C498FAA82D93),
        LE64_T(0x74FAE74749169BCE), LE64_T(0x4BF1060DD87CF605),
        LE64_T(0xE318EB5CF9EF77C4), LE64_T(0xDC130A1668851A0F),
        LE64_T(0x9D0F29C9DB3BAC52), LE64_T(0xA204C8834A51C199),
        LE64_T(0x1F376E76BC46C0E8), LE64_T(0x203C8F3C2D2CAD23),
        LE64_T(0x6120ACE39E921B7E), LE64_T(0x5E2B4DA90FF876B5),
        LE64_T(0x899F4E23DDB20719), LE64_T(0xB694AF694CD86AD2),
        LE64_T(0xF7888CB6FF66DC8F), LE64_T(0xC8836DFC6E0CB144),
        LE64_T(0x75B0CB09981BB035), LE64_T(0x4ABB2A430971DDFE),
        LE64_T(0x0BA7099CBACF6BA3), LE64_T(0x34ACE8D62BA50668),
        LE64_T(0x3617A1A2B155967E), LE64_T(0x091C40E8203FFBB5),
        LE64_T(0x4800633793814DE8), LE64_T(0x770B827D02EB2023),
        LE64_T(0xCA382488F4FC2152), LE64_T(0xF533C5C265964C99),
        LE64_T(0xB42FE61DD628FAC4), LE64_T(0x8B2407574742970F),
        LE64_T(0x5C9004DD9508E6A3), LE64_T(0x639BE59704628B68),
        LE64_T(0x2287C648B7DC3D35), LE64_T(0x1D8C270226B650FE),
        LE64_T(0xA0BF81F7D0A1518F), LE64_T(0x9FB460BD41CB3C44),
        LE64_T(0xDEA84362F2758A19), LE64_T(0xE1A3A228631FE7D2),
        LE64_T(0xDBDED18BC794AA35), LE64_T(0xE4D530C156FEC7FE),
        LE64_T(0xA5C9131EE54071A3), LE64_T(0x9AC2F254742A1C68),
        LE64_T(0x27F154A1823D1D19), LE64_T(0x18FAB5EB135770D2),
        LE64_T(0x59E69634A0E9C68F), LE64_T(0x66ED777E3183AB44),
        LE64_T(0xB15974F4E3C9DAE8), LE64_T(0x8E5295BE72A3B723),
        LE64_T(0xCF4EB661C11D017E), LE64_T(0xF045572B50776CB5),
        LE64_T(0x4D76F1DEA6606DC4), LE64_T(0x727D1094370A000F),
        LE64_T(0x3361334B84B4B652), LE64_T(0x0C6AD20115DEDB99),
        LE64_T(0x0ED19B758F2E4B8F), LE64_T(0x31DA7A3F1E442644),
        LE64_T(0x70C659E0ADFA9019), LE64_T(0x4FCDB8AA3C90FDD2),
        LE64_T(0xF2FE1E5FCA87FCA3), LE64_T(0xCDF5FF155BED9168),
        LE64_T(0x8CE9DCCAE8532735), LE64_T(0xB3E23D8079394AFE),
        LE64_T(0x64563E0AAB733B52), LE64_T(0x5B5DDF403A195699),
        LE64_T(0x1A41FC9F89A7E0C4), LE64_T(0x254A1DD518CD8D0F),
        LE64_T(0x9879BB20EEDA8C7E), LE64_T(0xA7725A6A7FB0E1B5),
        LE64_T(0xE66E79B5CC0E57E8), LE64_T(0xD96598FF5D643A23),
        LE64_T(0x92949EF28518CC26), LE64_T(0xAD9F7FB81472A1ED),
        LE64_T(0xEC835C67A7CC17B0), LE64_T(0xD388BD2D36A67A7B),
        LE64_T(0x6EBB1BD8C0B17B0A), LE64_T(0x51B0FA9251DB16C1),
        LE64_T(0x10ACD94DE265A09C), LE64_T(0x2FA73807730FCD57),
        LE64_T(0xF8133B8DA145BCFB), LE64_T(0xC718DAC7302FD130),
        LE64_T(0x8604F9188391676D), LE64_T(0xB90F185212FB0AA6),
        LE64_T(0x043CBEA7E4EC0BD7), LE64_T(0x3B375FED7586661C),
        LE64_T(0x7A2B7C32C638D041), LE64_T(0x45209D785752BD8A),
        LE64_T(0x479BD40CCDA22D9C), LE64_T(0x789035465CC84057),
        LE64_T(0x398C1699EF76F60A), LE64_T(0x0687F7D37E1C9BC1),
        LE64_T(0xBBB45126880B9AB0), LE64_T(0x84BFB06C1961F77B),
        LE64_T(0xC5A393B3AADF4126), LE64_T(0xFAA872F93BB52CED),
        LE64_T(0x2D1C7173E9FF5D41), LE64_T(0x121790397895308A),
        LE64_T(0x530BB3E6CB2B86D7), LE64_T(0x6C0052AC5A41EB1C),
        LE64_T(0xD133F459AC56EA6D), LE64_T(0xEE3815133D3C87A6),
        LE64_T(0xAF2436CC8E8231FB), LE64_T(0x902FD7861FE85C30),
        LE64_T(0xAA52A425BB6311D7), LE64_T(0x9559456F2A097C1C),
        LE64_T(0xD44566B099B7CA41), LE64_T(0xEB4E87FA08DDA78A),
        LE64_T(0x567D210FFECAA6FB), LE64_T(0x6976C0456FA0CB30),
        LE64_T(0x286AE39ADC1E7D6D), LE64_T(0x176102D04D7410A6),
        LE64_T(0xC0D5015A9F3E610A), LE64_T(0xFFDEE0100E540CC1),
        LE64_T(0xBEC2C3CFBDEABA9C), LE64_T(0x81C922852C80D757),
        LE64_T(0x3CFA8470DA97D626), LE64_T(0x03F1653A4BFDBBED),
        LE64_T(0x42ED46E5F8430DB0), LE64_T(0x7DE6A7AF6929607B),
        LE64_T(0x7F5DEEDBF3D9F06D), LE64_T(0x40560F9162B39DA6),
        LE64_T(0x014A2C4ED10D2BFB), LE64_T(0x3E41CD0440674630),
        LE64_T(0x83726BF1B6704741), LE64_T(0xBC798ABB271A2A8A),
        LE64_T(0xFD65A96494A49CD7), LE64_T(0xC26E482E05CEF11C),
        LE64_T(0x15DA4BA4D78480B0), LE64_T(0x2AD1AAEE46EEED7B),
        LE64_T(0x6BCD8931F5505B26), LE64_T(0x54C6687B643A36ED),
        LE64_T(0xE9F5CE8E922D379C), LE64_T(0xD6FE2FC403475A57),
        LE64_T(0x97E20C1BB0F9EC0A), LE64_T(0xA8E9ED51219381C1),
    }, {
        LE64_T(0x0000000000000000), LE64_T(0x1DEE8A5E222CA1DC),
        LE64_T(0x3BDD14BC445943B8), LE64_T(0x26339EE26675E264),
        LE64_T(0x77BA297888B28770), LE64_T(0x6A54A326AA9E26AC),
        LE64_T(0x4C673DC4CCEBC4C8), LE64_T(0x5189B79AEEC76514),
        LE64_T(0xEF7452F111650EE0), LE64_T(0xF29AD8AF3349AF3C),
        LE64_T(0xD4A9464D553C4D58), LE64_T(0xC947CC137710EC84),
        LE64_T(0x98CE7B8999D78990), LE64_T(0x8520F1D7BBFB284C),
        LE64_T(0xA3136F35DD8ECA28), LE64_T(0xBEFDE56BFFA26BF4),
        LE64_T(0x4C300AC98DC40345), LE64_T(0x51DE8097AFE8A299),
        LE64_T(0x77ED1E75C99D40FD), LE64_T(0x6A03942BEBB1E121),
        LE64_T(0x3B8A23B105768435), LE64_T(0x2664A9EF275A25E9),
        LE64_T(0x0057370D412FC78D), LE64_T(0x1DB9BD5363036651),
        LE64_T(0xA34458389CA10DA5), LE64_T(0xBEAAD266BE8DAC79),
        LE64_T(0x98994C84D8F84E1D), LE64_T(0x8577C6DAFAD4EFC1),
        LE64_T(0xD4FE714014138AD5), LE64_T(0xC910FB1E363F2B09),
        LE64_T(0xEF2365FC504AC96D), LE64_T(0xF2CDEFA2726668B1),
        LE64_T(0x986015931B88068A), LE64_T(0x858E9FCD39A4A756),
        LE64_T(0xA3BD012F5FD14532), LE64_T(0xBE538B717DFDE4EE),
        LE64_T(0xEFDA3CEB933A81FA), LE64_T(0xF234B6B5B1162026),
        LE64_T(0xD4072857D763C242), LE64_T(0xC9E9A209F54F639E),
        LE64_T(0x771447620AED086A), LE64_T(0x6AFACD3C28C1A9B6),
        LE64_T(0x4CC953DE4EB44BD2), LE64_T(0x5127D9806C98EA0E),
        LE64_T(0x00AE6E1A825F8F1A), LE64_T(0x1D40E444A0732EC6),
        LE64_T(0x3B737AA6C606CCA2), LE64_T(0x269DF0F8E42A6D7E),
        LE64_T(0xD4501F5A964C05CF), LE64_T(0xC9BE9504B460A413),
        LE64_T(0xEF8D0BE6D2154677), LE64_T(0xF26381B8F039E7AB),
        LE64_T(0xA3EA36221EFE82BF), LE64_T(0xBE04BC7C3CD22363),
        LE64_T(0x9837229E5AA7C107), LE64_T(0x85D9A8C0788B60DB),
        LE64_T(0x3B244DAB87290B2F), LE64_T(0x26CAC7F5A505AAF3),
        LE64_T(0x00F95917C3704897), LE64_T(0x1D17D349E15CE94B),
        LE64_T(0x4C9E64D30F9B8C5F), LE64_T(0x5170EE8D2DB72D83),
        LE64_T(0x7743706F4BC2CFE7), LE64_T(0x6AADFA3169EE6E3B),
        LE64_T(0xA218840D981E1391), LE64_T(0xBFF60E53BA32B24D),
        LE64_T(0x99C590B1DC475029), LE64_T(0x842B1AEFFE6BF1F5),
        LE64_T(0xD5A2AD7510AC94E1), LE64_T(0xC84C272B3280353D),
        LE64_T(0xEE7FB9C954F5D759), LE64_T(0xF391339776D97685),
        LE64_T(0x4D6CD6FC897B1D71), LE64_T(0x50825CA2AB57BCAD),
        LE64_T(0x76B1C240CD225EC9), LE64_T(0x6B5F481EEF0EFF15),
        LE64_T(0x3AD6FF8401C99A01), LE64_T(0x273875DA23E53BDD),
        LE64_T(0x010BEB384590D9B9), LE64_T(0x1CE5616667BC7865),
        LE64_T(0xEE288EC415DA10D4), LE64_T(0xF3C6049A37F6B108),
        LE64_T(0xD5F59A785183536C), LE64_T(0xC81B102673AFF2B0),
        LE64_T(0x9992A7BC9D6897A4), LE64_T(0x847C2DE2BF443678),
        LE64_T(0xA24FB300D931D41C), LE64_T(0xBFA1395EFB1D75C0),
        LE64_T(0x015CDC3504BF1E34), LE64_T(0x1CB2566B2693BFE8),
        LE64_T(0x3A81C88940E65D8C), LE64_T(0x276F42D762CAFC50),
        LE64_T(0x76E6F54D8C0D9944), LE64_T(0x6B087F13AE213898),
        LE64_T(0x4D3BE1F1C854DAFC), LE64_T(0x50D56BAFEA787B20),
        LE64_T(0x3A78919E8396151B), LE64_T(0x27961BC0A1BAB4C7),
        LE64_T(0x01A58522C7CF56A3), LE64_T(0x1C4B0F7CE5E3F77F),
        LE64_T(0x4DC2B8E60B24926B), LE64_T(0x502C32B8290833B7),
        LE64_T(0x761FAC5A4F7DD1D3), LE64_T(0x6BF126046D51700F),
        LE64_T(0xD50CC36F92F31BFB), LE64_T(0xC8E24931B0DFBA27),
        LE64_T(0xEED1D7D3D6AA5843), LE64_T(0xF33F5D8DF486F99F),
        LE64_T(0xA2B6EA171A419C8B), LE64_T(0xBF586049386D3D57),
        LE64_T(0x996BFEAB5E18DF33), LE64_T(0x848574F57C347EEF),
        LE64_T(0x76489B570E52165E), LE64_T(0x6BA611092C7EB782),
        LE64_T(0x4D958FEB4A0B55E6), LE64_T(0x507B05B56827F43A),
        LE64_T(0x01F2B22F86E0912E), LE64_T(0x1C1C3871A4CC30F2),
        LE64_T(0x3A2FA693C2B9D296), LE64_T(0x27C12CCDE095734A),
        LE64_T(0x993CC9A61F3718BE), LE64_T(0x84D243F83D1BB962),
        LE64_T(0xA2E1DD1A5B6E5B06), LE64_T(0xBF0F57447942FADA),
        LE64_T(0xEE86E0DE97859FCE), LE64_T(0xF3686A80B5A93E12),
        LE64_T(0xD55BF462D3DCDC76), LE64_T(0xC8B57E3CF1F07DAA),
        LE64_T(0xD6E9A7309F3239A7), LE64_T(0xCB072D6EBD1E987B),
        LE64_T(0xED34B38CDB6B7A1F), LE64_T(0xF0DA39D2F947DBC3),
        LE64_T(0xA1538E481780BED7), LE64_T(0xBCBD041635AC1F0B),
        LE64_T(0x9A8E9AF453D9FD6F), LE64_T(0x876010AA71F55CB3),
        LE64_T(0x399DF5C18E573747), LE64_T(0x24737F9FAC7B969B),
        LE64_T(0x0240E17DCA0E74FF), LE64_T(0x1FAE6B23E822D523),
        LE64_T(0x4E27DCB906E5B037), LE64_T(0x53C956E724C911EB),
        LE64_T(0x75FAC80542BCF38F), LE64_T(0x6814425B60905253),
        LE64_T(0x9AD9ADF912F63AE2), LE64_T(0x873727A730DA9B3E),
        LE64_T(0xA104B94556AF795A), LE64_T(0xBCEA331B7483D886),
        LE64_T(0xED6384819A44BD92), LE64_T(0xF08D0EDFB8681C4E),
        LE64_T(0xD6BE903DDE1DFE2A), LE64_T(0xCB501A63FC315FF6),
        LE64_T(0x75ADFF0803933402), LE64_T(0x6843755621BF95DE),
        LE64_T(0x4E70EBB447CA77BA), LE64_T(0x539E61EA65E6D666),
        LE64_T(0x0217D6708B21B372), LE64_T(0x1FF95C2EA90D12AE),
        LE64_T(0x39CAC2CCCF78F0CA), LE64_T(0x24244892ED545116),
        LE64_T(0x4E89B2A384BA3F2D), LE64_T(0x536738FDA6969EF1),
        LE64_T(0x7554A61FC0E37C95), LE64_T(0x68BA2C41E2CFDD49),
        LE64_T(0x39339BDB0C08B85D), LE64_T(0x24DD11852E241981),
        LE64_T(0x02EE8F674851FBE5), LE64_T(0x1F0005396A7D5A39),
        LE64_T(0xA1FDE05295DF31CD), LE64_T(0xBC136A0CB7F39011),
        LE64_T(0x9A20F4EED1867275), LE64_T(0x87CE7EB0F3AAD3A9),
        LE64_T(0xD647C92A1D6DB6BD), LE64_T(0xCBA943743F411761),
        LE64_T(0xED9ADD965934F505), LE64_T(0xF07457C87B1854D9),
        LE64_T(0x02B9B86A097E3C68), LE64_T(0x1F5732342B529DB4),
        LE64_T(0x3964ACD64D277FD0), LE64_T(0x248A26886F0BDE0C),
        LE64_T(0x7503911281CCBB18), LE64_T(0x68ED1B4CA3E01AC4),
        LE64_T(0x4EDE85AEC595F8A0), LE64_T(0x53300FF0E7B9597C),
        LE64_T(0xEDCDEA9B181B3288), LE64_T(0xF02360C53A379354),
        LE64_T(0xD610FE275C427130), LE64_T(0xCBFE74797E6ED0EC),
        LE64_T(0x9A77C3E390A9B5F8), LE64_T(0x879949BDB2851424),
        LE64_T(0xA1AAD75FD4F0F640), LE64_T(0xBC445D01F6DC579C),
        LE64_T(0x74F1233D072C2A36), LE64_T(0x691FA96325008BEA),
        LE64_T(0x4F2C37814375698E), LE64_T(0x52C2BDDF6159C852),
        LE64_T(0x034B0A458F9EAD46), LE64_T(0x1EA5801BADB20C9A),
        LE64_T(0x38961EF9CBC7EEFE), LE64_T(0x257894A7E9EB4F22),
        LE64_T(0x9B8571CC164924D6), LE64_T(0x866BFB923465850A),
        LE64_T(0xA05865705210676E), LE64_T(0xBDB6EF2E703CC6B2),
        LE64_T(0xEC3F58B49EFBA3A6), LE64_T(0xF1D1D2EABCD7027A),
        LE64_T(0xD7E24C08DAA2E01E), LE64_T(0xCA0CC656F88E41C2),
        LE64_T(0x38C129F48AE82973), LE64_T(0x252FA3AAA8C488AF),
        LE64_T(0x031C3D48CEB16ACB), LE64_T(0x1EF2B716EC9DCB17),
        LE64_T(0x4F7B008C025AAE03), LE64_T(0x52958AD220760FDF),
        LE64_T(0x74A614304603EDBB), LE64_T(0x69489E6E642F4C67),
        LE64_T(0xD7B57B059B8D2793), LE64_T(0xCA5BF15BB9A1864F),
        LE64_T(0xEC686FB9DFD4642B), LE64_T(0xF186E5E7FDF8C5F7),
        LE64_T(0xA00F527D133FA0E3), LE64_T(0xBDE1D8233113013F),
        LE64_T(0x9BD246C15766E35B), LE64_T(0x863CCC9F754A4287),
        LE64_T(0xEC9136AE1CA42CBC), LE64_T(0xF17FBCF03E888D60),
        LE64_T(0xD74C221258FD6F04), LE64_T(0xCAA2A84C7AD1CED8),
        LE64_T(0x9B2B1FD69416ABCC), LE64_T(0x86C59588B63A0A10),
        LE64_T(0xA0F60B6AD04FE874), LE64_T(0xBD188134F26349A8),
        LE64_T(0x03E5645F0DC1225C), LE64_T(0x1E0BEE012FED8380),
        LE64_T(0x383870E3499861E4), LE64_T(0x25D6FABD6BB4C038),
        LE64_T(0x745F4D278573A52C), LE64_T(0x69B1C779A75F04F0),
        LE64_T(0x4F82599BC12AE694), LE64_T(0x526CD3C5E3064748),
        LE64_T(0xA0A13C6791602FF9), LE64_T(0xBD4FB639B34C8E25),
        LE64_T(0x9B7C28DBD5396C41), LE64_T(0x8692A285F715CD9D),
        LE64_T(0xD71B151F19D2A889), LE64_T(0xCAF59F413BFE0955),
        LE64_T(0xECC601A35D8BEB31), LE64_T(0xF1288BFD7FA74AED),
        LE64_T(0x4FD56E9680052119), LE64_T(0x523BE4C8A22980C5),
        LE64_T(0x74087A2AC45C62A1), LE64_T(0x69E6F074E670C37D),
        LE64_T(0x386F47EE08B7A669), LE64_T(0x2581CDB02A9B07B5),
        LE64_T(0x03B253524CEEE5D1), LE64_T(0x1E5CD90C6EC2440D),
    }
};
