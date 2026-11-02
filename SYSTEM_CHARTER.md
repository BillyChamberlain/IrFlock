# AAAAAH (It Decides What You Are)

A face on a 0.96 in screen that wakes up, decides what you are, and panics about it. Built on `bench/scream_face`. The sketch is `bench/name_it/name_it.ino`.

## It sees

Distance in cm from the HC-SR04, about 20 readings a second.

| Who | Reading |
|---|---|
| Nobody | 400 (no echo comes back) |
| Walking past | about 90 |
| Hand at the screen | about 8 |

## It calls you

| Name | Number | Side | Hold |
|---|---|---|---|
| **MONSTER** | 15 cm | under | 1 s |
| **STRANGER** | 150 cm | under | 0.5 s |
| **ALONE** (nobody) | 150 cm | at or over | 5 s |

The rules are checked top to bottom and the first one that matches wins. The readings are smoothed, so one bad echo doesn't rename anybody.

## It does

| Name | Action (run by the Nano) |
|---|---|
| **MONSTER** | Full distorted scream on the speaker at max volume. The pitch climbs the longer you stay. The face shakes, the pupils shrink to pinpricks, and the top strip reads `MONSTER!!!` |
| **STRANGER** | Dead silent. The eyes go wide and dart around looking for you, the mouth is a tiny "o", and the top strip reads `who's there?` |
| **ALONE** | The face falls asleep with drifting z's. Every 20 s it lets out one tiny falling whimper and sheds a single tear. The top strip reads `alone.` |
