## TUNING

This guide is intended for system administrators to optimize Construct and
maximize its performance for their environment. This does not cover [BUILD](BUILD.md)
tuning, and it is expected that Construct is already installed and the [SETUP](SETUP.md)
has been completed.

- Some instructions may reference Construct's configuration system. This is
accessed via the administrator's console which can be reached by striking
`ctrl-c (SIGINT)` and then using the `conf` command (see: `help conf`). The
console can also be reached interactively through your preferred client in
the `!control` room. Alternatively, configuration state can be manipulated
directly through the `!conf` room. Configuration changes take effect as a
result of state events sent to the `!conf` room, thus all aforementioned
methods to change configuration are the same.

- CHANGES TO CONFIGURATION ARE EFFECTIVE IMMEDIATELY. ERRONEOUS VALUES MAY
CAUSE UNEXPECTED BEHAVIOR AND RESULT IN PROGRAM TERMINATION. CONFIGURATION
ERRORS MAY ALSO PREVENT STARTUP. Please see the
[TROUBLESHOOTING](TROUBLESHOOTING.md#recovering-from-broken-configurations)
guide for how to recover from configuration errors.


### Event Cache Tuning

Most of Construct's runtime footprint in RAM consists of a cache of Matrix
events read from the database. The data in many of these events may be
directly accessed for fundamental server operations; for example, a client's
access-token and user information is stored with events in special server
rooms. The event cache is a set of LRU (Least Recently Used) caches. The size
of these caches should be tuned to at least the "working-set size" expected
by the server. If these caches are too small, load will be placed
on the next storage tier. For storage devices with poor random access
characteristics it is important these caches cover the server's working-set
size.

To list the event cache information, try the following commands (example output
shown):

```
> db cache events *

COLUMN                               PCT       HITS    MISSES    INSERT                     CACHED                   CAPACITY               INSERT TOTAL               LOCKED
*                                 61.94%   18742243   3818637   3814446      1.41 GiB (1517280856)      2.28 GiB (2449473536)      4.46 GiB (4787594200)   4.41 MiB (4628512)
```

```
> db cache events **

COLUMN                               PCT       HITS    MISSES    INSERT                     CACHED                   CAPACITY               INSERT TOTAL               LOCKED
content                           17.85%    2113271     85256     83255       22.85 MiB (23962992)     128.00 MiB (134217728)     569.37 MiB (597026848)           0.00 B (0)
depth                             90.71%      11292     96431     96431       58.06 MiB (60876968)       64.00 MiB (67108864)       59.68 MiB (62575248)           0.00 B (0)
event_id                           9.24%     191518    153523    153523         5.92 MiB (6202768)       64.00 MiB (67108864)     865.07 MiB (907093240)           0.00 B (0)
origin_server_ts                  99.99%       9852    566483    566258       64.00 MiB (67103832)       64.00 MiB (67108864)     353.29 MiB (370455584)           0.00 B (0)
room_id                           99.99%    1015939    216695    216694       63.99 MiB (67102496)       64.00 MiB (67108864)     132.05 MiB (138467768)   1.93 MiB (2019088)
sender                            39.18%      56357     80879     80879       50.16 MiB (52592768)     128.00 MiB (134217728)       50.36 MiB (52809616)           0.00 B (0)
state_key                         40.49%       7336     89035     87181       25.91 MiB (27171856)       64.00 MiB (67108864)     383.42 MiB (402049648)           0.00 B (0)
type                              99.92%    1716885     66485     66485       31.97 MiB (33527264)       32.00 MiB (33554432)       40.69 MiB (42667312)           0.00 B (0)
_event_idx                        99.99%     652575    505956    505955     255.98 MiB (268418416)     256.00 MiB (268435456)     635.40 MiB (666268064)    23.45 KiB (24016)
_room_events                      62.14%     308312     13144     13144       79.54 MiB (83405864)     128.00 MiB (134217728)       79.73 MiB (83608112)  284.73 KiB (291560)
_room_joined                      52.73%    2087968      6789      6789         4.22 MiB (4422936)         8.00 MiB (8388608)         4.23 MiB (4431280)           0.00 B (0)
_room_state                       25.40%    2038549     21590     21590       16.25 MiB (17044504)       64.00 MiB (67108864)       52.26 MiB (54793600)           0.00 B (0)
_room_head                        26.41%       7986      9435      9435         2.11 MiB (2215192)         8.00 MiB (8388608)       37.56 MiB (39389688)           0.00 B (0)
_event_json                       62.79%      82254   1166164   1166153     642.96 MiB (674189112)   1024.00 MiB (1073741824)     736.76 MiB (772552224)   3.52 MiB (3690824)
_event_refs                       79.17%      54501    112508    112505       50.67 MiB (53127080)       64.00 MiB (67108864)       68.76 MiB (72098088)           0.00 B (0)
_event_type                       99.77%         22      8215      8215       15.96 MiB (16738848)       16.00 MiB (16777216)       17.27 MiB (18109240)    73.93 KiB (75704)
_event_sender                      0.00%          0     23453     23453                 0.00 B (0)       16.00 MiB (16777216)       15.01 MiB (15739768)           0.00 B (0)
_event_horizon                    99.96%      15722     18296     18296       15.99 MiB (16769768)       16.00 MiB (16777216)       18.91 MiB (19833200)           0.00 B (0)
_room_state_space                 67.24%       3997     24712     24712       86.06 MiB (90241400)     128.00 MiB (134217728)       92.28 MiB (96762256)           0.00 B (0)
```

To view the configuration item for the size of a cache, which should match your
output from the above command, use the following command where `<COLUMN>` is
replaced by one of the names under `COLUMN` in the above output:

```
conf ircd.m.dbs.<COLUMN>.cache.size
```

To alter a cache size, set the configuration item with a byte value. In the
example below we will set the `_event_json` cache size to 256 MiB. This change
will take effect immediately and the cache will grow or shrink to that size.

```
conf set ircd.m.dbs._event_json.cache.size 268435456
```

> Tip: The best metric to figure out which caches are inadequate is not
necessarily the utilization percentage. Caches that are too small generally
exhibit high values under `INSERT TOTAL` as well as full utilization. If this
value is several times higher than the cache size and growing, consider
increasing that cache's size.


### Client Pool Tuning

(TODO)
