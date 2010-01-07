/*
 * libString, Copyright (C) 1999 Patrick Alken
 * This library comes with absolutely NO WARRANTY
 *
 * Should you choose to use and/or modify this source code, please
 * do so under the terms of the GNU General Public License under which
 * this library is distributed.
 *
 * $Id: rsdb_snprintf.c 26094 2008-09-19 15:33:46Z androsyn $
 */
#include "stdinc.h"
#include "rsdb.h"

/*
 *  This table is arranged in chronological order from 0-999,
 * however the numbers are written backwards, so the number 100
 * is expressed in this table as "001".
 *  It's purpose is to ensure fast conversions from integers to
 * ASCII strings. When an integer variable is encountered, a
 * simple hash algorithm is used to determine where to look
 * in this array for the corresponding string.
 *  This outperforms continually dividing by 10 and using the
 * digit obtained as a character, because we can now divide by
 * 1000 and use the remainder directly, thus cutting down on
 * the number of costly divisions needed. For an integer's worst
 * case, 2 divisions are needed because it can only go up to
 * 32767, so after 2 divisions by 1000, and some algebra, we will
 * be left with 327 which we can get from this table. This is much
 * better than the 5 divisions by 10 that we would need if we did
 * it the conventional way. Of course, if we made this table go
 * from 0-9999, only 1 division would be needed.
 *  Longs and unsigned ints of course, are another matter :-).
 *
 * Patrick Alken <wnder@underworld.net>
 */

/*
 * Set this to the number of indices (numbers) in our table
 */
#define TABLE_MAX    1000

static const char *IntTable[] = {
	"000", "100", "200", "300", "400",
	"500", "600", "700", "800", "900",
	"010", "110", "210", "310", "410",
	"510", "610", "710", "810", "910",
	"020", "120", "220", "320", "420",
	"520", "620", "720", "820", "920",
	"030", "130", "230", "330", "430",
	"530", "630", "730", "830", "930",
	"040", "140", "240", "340", "440",
	"540", "640", "740", "840", "940",
	"050", "150", "250", "350", "450",
	"550", "650", "750", "850", "950",
	"060", "160", "260", "360", "460",
	"560", "660", "760", "860", "960",
	"070", "170", "270", "370", "470",
	"570", "670", "770", "870", "970",
	"080", "180", "280", "380", "480",
	"580", "680", "780", "880", "980",
	"090", "190", "290", "390", "490",
	"590", "690", "790", "890", "990",
	"001", "101", "201", "301", "401",
	"501", "601", "701", "801", "901",
	"011", "111", "211", "311", "411",
	"511", "611", "711", "811", "911",
	"021", "121", "221", "321", "421",
	"521", "621", "721", "821", "921",
	"031", "131", "231", "331", "431",
	"531", "631", "731", "831", "931",
	"041", "141", "241", "341", "441",
	"541", "641", "741", "841", "941",
	"051", "151", "251", "351", "451",
	"551", "651", "751", "851", "951",
	"061", "161", "261", "361", "461",
	"561", "661", "761", "861", "961",
	"071", "171", "271", "371", "471",
	"571", "671", "771", "871", "971",
	"081", "181", "281", "381", "481",
	"581", "681", "781", "881", "981",
	"091", "191", "291", "391", "491",
	"591", "691", "791", "891", "991",
	"002", "102", "202", "302", "402",
	"502", "602", "702", "802", "902",
	"012", "112", "212", "312", "412",
	"512", "612", "712", "812", "912",
	"022", "122", "222", "322", "422",
	"522", "622", "722", "822", "922",
	"032", "132", "232", "332", "432",
	"532", "632", "732", "832", "932",
	"042", "142", "242", "342", "442",
	"542", "642", "742", "842", "942",
	"052", "152", "252", "352", "452",
	"552", "652", "752", "852", "952",
	"062", "162", "262", "362", "462",
	"562", "662", "762", "862", "962",
	"072", "172", "272", "372", "472",
	"572", "672", "772", "872", "972",
	"082", "182", "282", "382", "482",
	"582", "682", "782", "882", "982",
	"092", "192", "292", "392", "492",
	"592", "692", "792", "892", "992",
	"003", "103", "203", "303", "403",
	"503", "603", "703", "803", "903",
	"013", "113", "213", "313", "413",
	"513", "613", "713", "813", "913",
	"023", "123", "223", "323", "423",
	"523", "623", "723", "823", "923",
	"033", "133", "233", "333", "433",
	"533", "633", "733", "833", "933",
	"043", "143", "243", "343", "443",
	"543", "643", "743", "843", "943",
	"053", "153", "253", "353", "453",
	"553", "653", "753", "853", "953",
	"063", "163", "263", "363", "463",
	"563", "663", "763", "863", "963",
	"073", "173", "273", "373", "473",
	"573", "673", "773", "873", "973",
	"083", "183", "283", "383", "483",
	"583", "683", "783", "883", "983",
	"093", "193", "293", "393", "493",
	"593", "693", "793", "893", "993",
	"004", "104", "204", "304", "404",
	"504", "604", "704", "804", "904",
	"014", "114", "214", "314", "414",
	"514", "614", "714", "814", "914",
	"024", "124", "224", "324", "424",
	"524", "624", "724", "824", "924",
	"034", "134", "234", "334", "434",
	"534", "634", "734", "834", "934",
	"044", "144", "244", "344", "444",
	"544", "644", "744", "844", "944",
	"054", "154", "254", "354", "454",
	"554", "654", "754", "854", "954",
	"064", "164", "264", "364", "464",
	"564", "664", "764", "864", "964",
	"074", "174", "274", "374", "474",
	"574", "674", "774", "874", "974",
	"084", "184", "284", "384", "484",
	"584", "684", "784", "884", "984",
	"094", "194", "294", "394", "494",
	"594", "694", "794", "894", "994",
	"005", "105", "205", "305", "405",
	"505", "605", "705", "805", "905",
	"015", "115", "215", "315", "415",
	"515", "615", "715", "815", "915",
	"025", "125", "225", "325", "425",
	"525", "625", "725", "825", "925",
	"035", "135", "235", "335", "435",
	"535", "635", "735", "835", "935",
	"045", "145", "245", "345", "445",
	"545", "645", "745", "845", "945",
	"055", "155", "255", "355", "455",
	"555", "655", "755", "855", "955",
	"065", "165", "265", "365", "465",
	"565", "665", "765", "865", "965",
	"075", "175", "275", "375", "475",
	"575", "675", "775", "875", "975",
	"085", "185", "285", "385", "485",
	"585", "685", "785", "885", "985",
	"095", "195", "295", "395", "495",
	"595", "695", "795", "895", "995",
	"006", "106", "206", "306", "406",
	"506", "606", "706", "806", "906",
	"016", "116", "216", "316", "416",
	"516", "616", "716", "816", "916",
	"026", "126", "226", "326", "426",
	"526", "626", "726", "826", "926",
	"036", "136", "236", "336", "436",
	"536", "636", "736", "836", "936",
	"046", "146", "246", "346", "446",
	"546", "646", "746", "846", "946",
	"056", "156", "256", "356", "456",
	"556", "656", "756", "856", "956",
	"066", "166", "266", "366", "466",
	"566", "666", "766", "866", "966",
	"076", "176", "276", "376", "476",
	"576", "676", "776", "876", "976",
	"086", "186", "286", "386", "486",
	"586", "686", "786", "886", "986",
	"096", "196", "296", "396", "496",
	"596", "696", "796", "896", "996",
	"007", "107", "207", "307", "407",
	"507", "607", "707", "807", "907",
	"017", "117", "217", "317", "417",
	"517", "617", "717", "817", "917",
	"027", "127", "227", "327", "427",
	"527", "627", "727", "827", "927",
	"037", "137", "237", "337", "437",
	"537", "637", "737", "837", "937",
	"047", "147", "247", "347", "447",
	"547", "647", "747", "847", "947",
	"057", "157", "257", "357", "457",
	"557", "657", "757", "857", "957",
	"067", "167", "267", "367", "467",
	"567", "667", "767", "867", "967",
	"077", "177", "277", "377", "477",
	"577", "677", "777", "877", "977",
	"087", "187", "287", "387", "487",
	"587", "687", "787", "887", "987",
	"097", "197", "297", "397", "497",
	"597", "697", "797", "897", "997",
	"008", "108", "208", "308", "408",
	"508", "608", "708", "808", "908",
	"018", "118", "218", "318", "418",
	"518", "618", "718", "818", "918",
	"028", "128", "228", "328", "428",
	"528", "628", "728", "828", "928",
	"038", "138", "238", "338", "438",
	"538", "638", "738", "838", "938",
	"048", "148", "248", "348", "448",
	"548", "648", "748", "848", "948",
	"058", "158", "258", "358", "458",
	"558", "658", "758", "858", "958",
	"068", "168", "268", "368", "468",
	"568", "668", "768", "868", "968",
	"078", "178", "278", "378", "478",
	"578", "678", "778", "878", "978",
	"088", "188", "288", "388", "488",
	"588", "688", "788", "888", "988",
	"098", "198", "298", "398", "498",
	"598", "698", "798", "898", "998",
	"009", "109", "209", "309", "409",
	"509", "609", "709", "809", "909",
	"019", "119", "219", "319", "419",
	"519", "619", "719", "819", "919",
	"029", "129", "229", "329", "429",
	"529", "629", "729", "829", "929",
	"039", "139", "239", "339", "439",
	"539", "639", "739", "839", "939",
	"049", "149", "249", "349", "449",
	"549", "649", "749", "849", "949",
	"059", "159", "259", "359", "459",
	"559", "659", "759", "859", "959",
	"069", "169", "269", "369", "469",
	"569", "669", "769", "869", "969",
	"079", "179", "279", "379", "479",
	"579", "679", "779", "879", "979",
	"089", "189", "289", "389", "489",
	"589", "689", "789", "889", "989",
	"099", "199", "299", "399", "499",
	"599", "699", "799", "899", "999"
};

/*
 * Since we calculate the right-most digits for %d %u etc first,
 * we need a temporary buffer to store them in until we get
 * to the left-most digits
 */

#define TEMPBUF_MAX  20

static char TempBuffer[TEMPBUF_MAX];

/*
vSnprintf()
 Backend to Snprintf() - performs the construction of 'dest'
using the string 'format' and the given arguments. Also makes sure
not more than 'bytes' characters are copied to 'dest'

 We always allow room for a terminating \0 character, so at most,
bytes - 1 characters will be written to dest.

Return: Number of characters written, NOT including the terminating
        \0 character which is *always* placed at the end of the string

NOTE: This function handles the following flags only:
        %s %d %c %u %ld %lu
      In addition, this function performs *NO* precision, padding,
      or width formatting. If it receives an unknown % character,
      it will call vsprintf() to complete the remainder of the
      string.
*/

int
rs_vsnprintf(char *dest, const size_t bytes, const char *format, va_list args)
{
	char ch;
	int written = 0;	/* bytes written so far */
	int maxbytes = bytes - 1;

	while((ch = *format++) && (written < maxbytes))
	{
		if(ch == '%')
		{
			/*
			 * Advance past the %
			 */
			ch = *format++;

			/*
			 * Put the most common cases first - %s %d etc
			 */

			if(ch == 's')
			{
				const char *str = va_arg(args, const char *);

				while((*dest = *str))
				{
					++dest;
					++str;

					if(++written >= maxbytes)
						break;
				}

				continue;
			}

			if(ch == 'd')
			{
				int num = va_arg(args, int);
				int quotient;
				const char *str;
				char *digitptr = TempBuffer;

				/*
				 * We have to special-case "0" unfortunately
				 */
				if(num == 0)
				{
					*dest++ = '0';
					++written;
					continue;
				}

				if(num < 0)
				{
					*dest++ = '-';
					if(++written >= maxbytes)
						continue;

					num = -num;
				}

				do
				{
					quotient = num / TABLE_MAX;

					/*
					 * We'll start with the right-most digits of 'num'.
					 * Dividing by TABLE_MAX cuts off all but the X
					 * right-most digits, where X is such that:
					 *
					 *     10^X = TABLE_MAX
					 *
					 * For example, if num = 1200, and TABLE_MAX = 1000,
					 * quotient will be 1. Multiplying this by 1000 and
					 * subtracting from 1200 gives: 1200 - (1 * 1000) = 200.
					 * We then go right to slot 200 in our array and behold!
					 * The string "002" (200 backwards) is conveniently
					 * waiting for us. Then repeat the process with the
					 * digits left.
					 *
					 * The reason we need to have the integers written
					 * backwards, is because we don't know how many digits
					 * there are. If we want to express the number 12130
					 * for example, our first pass would leave us with 130,
					 * whose slot in the array yields "031", which we
					 * plug into our TempBuffer[]. The next pass gives us
					 * 12, whose slot yields "21" which we append to
					 * TempBuffer[], leaving us with "03121". This is the
					 * exact number we want, only backwards, so it is
					 * a simple matter to reverse the string. If we used
					 * straightfoward numbers, we would have a TempBuffer
					 * looking like this: "13012" which would be a nightmare
					 * to deal with.
					 */

					str = IntTable[num - (quotient * TABLE_MAX)];

					while((*digitptr = *str))
					{
						++digitptr;
						++str;
					}
				}
				while((num = quotient) != 0);

				/*
				 * If the last quotient was a 1 or 2 digit number, there
				 * will be one or more leading zeroes in TempBuffer[] -
				 * get rid of them.
				 */
				while(*(digitptr - 1) == '0')
					--digitptr;

				while(digitptr != TempBuffer)
				{
					*dest++ = *--digitptr;
					if(++written >= maxbytes)
						break;
				}

				continue;
			}	/* if (ch == 'd') */

			if(ch == 'c')
			{
				*dest++ = va_arg(args, int);

				++written;

				continue;
			}	/* if (ch == 'c') */

			if(ch == 'u')
			{
				unsigned int num = va_arg(args, unsigned int);
				unsigned int quotient;
				const char *str;
				char *digitptr = TempBuffer;

				if(num == 0)
				{
					*dest++ = '0';
					++written;
					continue;
				}

				do
				{
					quotient = num / TABLE_MAX;

					/*
					 * Very similar to case 'd'
					 */

					str = IntTable[num - (quotient * TABLE_MAX)];

					while((*digitptr = *str))
					{
						++digitptr;
						++str;
					}
				}
				while((num = quotient) != 0);

				while(*(digitptr - 1) == '0')
					--digitptr;

				while(digitptr != TempBuffer)
				{
					*dest++ = *--digitptr;
					if(++written >= maxbytes)
						break;
				}

				continue;
			}	/* if (ch == 'u') */

			if(ch == 'Q')
			{
				const char *arg = va_arg(args, const char *);

				if(arg == NULL)
					continue;

				const char *str = rsdb_quote(arg);

				while((*dest = *str))
				{
					++dest;
					++str;

					if(++written >= maxbytes)
						break;
				}

				continue;
			}

			if(ch == 'l')
			{
				if(*format == 'u')
				{
					unsigned long num = va_arg(args, unsigned long);
					unsigned long quotient;
					const char *str;
					char *digitptr = TempBuffer;

					++format;

					if(num == 0)
					{
						*dest++ = '0';
						++written;
						continue;
					}

					do
					{
						quotient = num / TABLE_MAX;

						/*
						 * Very similar to case 'u'
						 */

						str = IntTable[num - (quotient * TABLE_MAX)];

						while((*digitptr = *str))
						{
							++digitptr;
							++str;
						}
					}
					while((num = quotient) != 0);

					while(*(digitptr - 1) == '0')
						--digitptr;

					while(digitptr != TempBuffer)
					{
						*dest++ = *--digitptr;
						if(++written >= maxbytes)
							break;
					}

					continue;
				}
				else
					/* if (*format == 'u') */ if(*format == 'd')
				{
					long num = va_arg(args, long);
					long quotient;
					const char *str;
					char *digitptr = TempBuffer;

					++format;

					if(num == 0)
					{
						*dest++ = '0';
						++written;
						continue;
					}

					if(num < 0)
					{
						*dest++ = '-';
						if(++written >= maxbytes)
							continue;

						num = -num;
					}

					do
					{
						quotient = num / TABLE_MAX;

						str = IntTable[num - (quotient * TABLE_MAX)];

						while((*digitptr = *str))
						{
							++digitptr;
							++str;
						}
					}
					while((num = quotient) != 0);

					while(*(digitptr - 1) == '0')
						--digitptr;

					while(digitptr != TempBuffer)
					{
						*dest++ = *--digitptr;
						if(++written >= maxbytes)
							break;
					}

					continue;
				}
				else	/* if (*format == 'd') */
				{
					/* XXX error */
					exit(1);
				}


			}	/* if (ch == 'l') */

			if(ch != '%')
			{
				/* XXX error */
				exit(1);
			}	/* if (ch != '%') */
		}		/* if (ch == '%') */

		*dest++ = ch;
		++written;
	}			/* while ((ch = *format++) && (written < maxbytes)) */

	/*
	 * Terminate the destination buffer with a \0
	 */
	*dest = '\0';

	return (written);
}				/* vSnprintf() */

/*
rs_snprintf()
 Optimized version of snprintf().

Inputs: dest   - destination string
        bytes  - number of bytes to copy
        format - formatted string
        args   - args to 'format'

Return: number of characters copied, NOT including the terminating
        NULL which is always placed at the end of the string
*/

int
rs_snprintf(char *dest, const size_t bytes, const char *format, ...)
{
	va_list args;
	int count;

	va_start(args, format);

	count = rs_vsnprintf(dest, bytes, format, args);

	va_end(args);

	return (count);
}				/* Snprintf() */
