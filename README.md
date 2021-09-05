# bilimessenger

> Tools for bilibili message management

#### Build Setup

``` bash
# install dependencies
npm install

# serve with hot reload at localhost:9080
npm run dev

# build electron application for production
npm run build


# lint all JS/Vue component files in `src/`
npm run lint

```

https://api.bilibili.com/x/space/acc/info?mid={uid}

```json
{
   "code":0,
   "message":"0",
   "ttl":1,
   "data":{
      "mid":475210,
      "name":"Xinrea",
      "sex":"保密",
      "face":"http://i1.hdslb.com/bfs/face/c4b1e83e8d1ea2bf95cd10528dc34ba80c77677d.jpg",
      "sign":"阿轴加油",
      "rank":10000,
      "level":6,
      "jointime":0,
      "moral":0,
      "silence":0,
      "coins":0,
      "fans_badge":false,
      "fans_medal":{
         "show":true,
         "wear":true,
         "medal":{
            "uid":475210,
            "target_id":61639371,
            "medal_id":163211,
            "level":27,
            "medal_name":"轴芯",
            "medal_color":398668,
            "intimacy":52558,
            "next_intimacy":90000,
            "day_limit":250000,
            "today_feed":1040,
            "medal_color_start":398668,
            "medal_color_end":6850801,
            "medal_color_border":16771156,
            "is_lighted":1,
            "guard_level":2,
            "light_status":1,
            "wearing_status":1,
            "score":50122558
         }
      },
      "official":{
         "role":0,
         "title":"",
         "desc":"",
         "type":-1
      },
      "vip":{
         "type":2,
         "status":1,
         "due_date":1661616000000,
         "vip_pay_type":1,
         "theme_type":0,
         "label":{
            "path":"",
            "text":"年度大会员",
            "label_theme":"annual_vip",
            "text_color":"#FFFFFF",
            "bg_style":1,
            "bg_color":"#FB7299",
            "border_color":""
         },
         "avatar_subscript":1,
         "nickname_color":"#FB7299",
         "role":3,
         "avatar_subscript_url":"http://i0.hdslb.com/bfs/vip/icon_Certification_big_member_22_3x.png"
      },
      "pendant":{
         "pid":201007,
         "name":"大头",
         "image":"http://i1.hdslb.com/bfs/garb/9ccb8f1b8442d92f148a63d3296a361356fc9bd2.png",
         "expire":0,
         "image_enhance":"http://i1.hdslb.com/bfs/garb/9ccb8f1b8442d92f148a63d3296a361356fc9bd2.png",
         "image_enhance_frame":""
      },
      "nameplate":{
         "nid":0,
         "name":"",
         "image":"",
         "image_small":"",
         "level":"",
         "condition":""
      },
      "user_honour_info":{
         "mid":0,
         "colour":null,
         "tags":null
      },
      "is_followed":false,
      "top_photo":"http://i0.hdslb.com/bfs/space/cb1c3ef50e22b6096fde67febe863494caefebad.png",
      "theme":{
         
      },
      "sys_notice":{
         
      },
      "live_room":{
         "roomStatus":1,
         "liveStatus":0,
         "url":"https://live.bilibili.com/843610",
         "title":"组内抽奖",
         "cover":"http://i0.hdslb.com/bfs/live-key-frame/keyframe0811210100000084361099xihl.jpg",
         "online":2,
         "roomid":843610,
         "roundStatus":0,
         "broadcast_type":0
      },
      "birthday":"",
      "school":{
         "name":""
      },
      "profession":{
         "name":""
      },
      "tags":null,
      "series":{
         "user_upgrade_status":3,
         "show_upgrade_window":false
      }
   }
}
```