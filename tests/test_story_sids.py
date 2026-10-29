#!/usr/bin/env python3
"""The permanent scene numbers (tables/story/sids.json, session 25): save v5 keeps the seen flags and each file's story
position by these numbers, so they must never change. This test freezes the numbers given so far: a number may be added
(the next free one), never changed or reused; every scene of scenes.json has one, or (a character variant, session 25b)
names its base scene in sid_of: same json frame and g4, the base numbered, so it shares the base's number; all are below
STORY_MAX (64).
usage: test_story_sids.py"""
import json, os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
T = os.path.join(HERE, '..', 'tables', 'story')
FROZEN = {
 "adv_opening": 0,
 "dv_meet": 1,
 "dv_crash": 2,
 "dv_crash2": 3,
 "rm_gong": 4,
 "rm_hideout": 5,
 "rm_collapse": 6,
 "st_bad_news": 7,
 "rm_meet_milla": 8,
 "th_torque_revealed": 9,
 "sg_brevon_plan": 10,
 "th_next_morning": 11,
 "np_gong_help": 12,
 "sm_spade": 13,
 "fn_panther_defeated": 14,
 "sm_zao_dinner": 15,
 "sg_intruders": 16,
 "sm_zao_help": 17,
 "zao_airship": 18,
 "zao_airship_ambush": 19,
 "jiang_river": 20,
 "st_magister_talk": 21,
 "jail_plan": 22,
 "jail_fail": 23,
 "throne_apology": 24,
 "sg_brevon_torque": 25,
 "jc_cave": 26,
 "jc_cave_cm": 27,
 "th_carol_help": 28,
 "tb_torture": 29,
 "tb_team_spade": 30,
 "tb_brevon": 31,
 "tb_arrest": 32,
 "tb_outside_team": 33,
 "st_mission": 34,
 "pl_final_message": 35,
 "sf_army": 36,
 "sf_camp": 37,
 "ending": 38,
 "game_intro": 39
}
sid = json.load(open(os.path.join(T, 'sids.json'), encoding='utf-8'))['sid']
SC = {s['id']: s for s in json.load(open(os.path.join(T, 'scenes.json'), encoding='utf-8'))['scenes']}
json_of = lambda r: r // 100 if r >= 1000 else r
def variant_ok(i):   # a variant (sid_of): its base is numbered and is the same json frame and g4
    b = SC[i].get('sid_of'); return b in sid and b in SC and json_of(SC[b]['room']) == json_of(SC[i]['room']) and SC[b]['g4'] == SC[i]['g4']
scenes = [i for i in SC if i in sid or not variant_ok(i)]   # the variants share their base's number
fails = 0
def check(name, cond, info=''):
    global fails
    print(('PASS ' if cond else 'FAIL ') + name + ('  [' + str(info) + ']' if info else ''))
    fails += 0 if cond else 1
check('frozen_numbers_unchanged', all(sid.get(k) == v for k, v in FROZEN.items()),
      [k for k, v in FROZEN.items() if sid.get(k) != v])
check('every_scene_numbered', all(s in sid for s in scenes), [s for s in scenes if s not in sid])
check('numbers_unique', len(set(sid.values())) == len(sid))
check('numbers_below_64', all(0 <= v < 64 for v in sid.values()), max(sid.values()))
print('%d failures' % fails)
sys.exit(1 if fails else 0)
