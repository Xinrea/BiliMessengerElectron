import * as https from 'https'
import * as http from 'http'
import querystring from 'querystring'

export function getUserInfo(userData, mid) {
  return new Promise((resolve, reject) => {
    try {
      fetch('https://api.bilibili.com/x/space/acc/info?mid='+mid+'&jsonp=jsonp&platform=web', {
        "headers": {
          "accept": "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.9",
          "accept-language": "zh-CN,zh;q=0.9",
          "cache-control": "max-age=0",
          "sec-ch-ua": "\"Google Chrome\";v=\"105\", \"Not)A;Brand\";v=\"8\", \"Chromium\";v=\"105\"",
          "sec-ch-ua-mobile": "?0",
          "sec-ch-ua-platform": "\"Windows\"",
          "sec-fetch-dest": "document",
          "sec-fetch-mode": "navigate",
          "sec-fetch-site": "none",
          "sec-fetch-user": "?1",
          "upgrade-insecure-requests": "1",
          'cookie':'SESSDATA='+userData.SESSDATA
        },
        "body": null,
        "method": "GET"
      }).then((raw)=>{
        raw.json().then(resp=>{
          if (resp.code === 0) {
            resolve(resp.data)
          } else {
            reject(resp)
          }
        }).catch(e=>{
          reject(e)
        })
      })
    }catch (e) {
      reject(e)
    }
  })
}

export function getRoomInfo(roomID) {
  // https://api.live.bilibili.com/xlive/web-room/v1/index/getH5InfoByRoom?RoomID=
  return new Promise((resolve, reject) => {
    try {
      if (roomID === '') {
        reject('Invalid Room ID')
      }
      https.get(
        {
          hostname: 'api.live.bilibili.com',
          path: '/xlive/web-room/v1/index/getH5InfoByRoom?room_id=' + roomID,
        },
        (res) => {
          let dd = ''
          res.on('data', (chunk) => {
            dd += chunk
          })
          res.on('end', () => {
            let resp = JSON.parse(dd.toString())
            if (resp['code'] === 0) {
              resolve(resp['data'])
            } else {
              reject('/xlive/web-room/v1/index/getH5InfoByRoom?room_id=' + roomID)
            }
          })
        }
      )
    }catch (e) {
      reject(e)
    }
  })
}

export function getFollowerHistory(uid) {
  return new Promise((resolve, reject)=>{
    try {
      https.get('https://api.vtbs.moe/v2/bulkActiveSome/' + uid, res => {
        let dd = ''
        res.on('data', chunk => {
          dd += chunk
        })
        res.on('end', () => {
          let resp = JSON.parse(dd.toString())
          resolve(resp)
        })
        res.on('error', err => {
          reject(err)
        })
      })
    }catch (e) {
      reject(e)
    }
  })
}

//https://api.vtbs.moe/v2/bulkGuard/61639371
export function getGuardHistory(uid) {
  return new Promise((resolve, reject)=>{
    try {
      https.get('https://api.vtbs.moe/v2/bulkGuard/' + uid, res => {
        let dd = ''
        res.on('data', chunk => {
          dd += chunk
        })
        res.on('end', () => {
          let resp = JSON.parse(dd.toString())
          resolve(resp)
        })
        res.on('error', err => {
          reject(err)
        })
      })
    }catch (e) {
      reject(e)
    }
  })
}

export function getGuardList(uid, page, page_size) {
  return new Promise((resolve, reject)=>{
    try {
      https.get('https://api.live.bilibili.com/xlive/app-room/v2/guardTab/topList?roomid=1'+'&page='+page+'&page_size='+page_size+'&ruid='+uid, res => {
        let dd = ''
        res.on('data', chunk => {
          dd += chunk
        })
        res.on('end', () => {
          let resp = JSON.parse(dd.toString())
          if (resp.code === 0) {
            resolve(resp.data)
          } else {
            reject(resp)
          }
        })
        res.on('error', err => {
          reject(err)
        })
      })
    }catch (e) {
      reject(e)
    }
  })
}

export function getGuardValidDate(rid) {
  return new Promise((resolve, reject)=>{
    try {
      http.get('http://guard.vjoi.cn/day?room='+rid, res => {
        let dd = ''
        res.on('data', chunk => {
          dd += chunk
        })
        res.on('end', () => {
          let resp = JSON.parse(dd.toString())
          if (resp.Code === 0) {
            resolve(resp.Data)
          } else {
            reject(resp)
          }
        })
        res.on('error', err => {
          reject(err)
        })
      })
    }catch (e) {
      reject(e)
    }
  })
}

export function getGuardHistoryList(rid, date) {
  return new Promise((resolve, reject)=>{
    try {
      http.get('http://guard.vjoi.cn/history?room='+rid+'&date='+date, res => {
        let dd = ''
        res.on('data', chunk => {
          dd += chunk
        })
        res.on('end', () => {
          let resp = JSON.parse(dd.toString())
          if (resp.Code === 0) {
            resolve(resp.Data)
          } else {
            reject(resp)
          }
        })
        res.on('error', err => {
          reject(err)
        })
      })
    }catch (e) {
      reject(e)
    }
  })
}

export function sendMessage(target, userData, content) {
  console.log(target, userData, content)
  return new Promise((resolve, reject)=>{
    try {
      let options = {
        hostname: 'api.vc.bilibili.com',
        path: '/web_im/v1/web_im/send_msg',
        port: 443,
        method: 'POST',
        headers: {
          'Content-Type':'application/x-www-form-urlencoded',
          'cookie':'SESSDATA='+userData.SESSDATA
        }
      }
      let req = https.request(options, res => {
        let dd = ''
        res.on('data', chunk => {
          dd += chunk
        })
        res.on('end', () => {
          let resp = JSON.parse(dd.toString())
          if (resp.code === 0) {
            resolve(resp.data)
          } else {
            reject(resp)
          }
        })
        res.on('error', err => {
          reject(err)
        })
      })
      let postData = querystring.stringify({
        'msg[sender_uid]':userData.DedeUserID,
        'msg[receiver_id]':target,
        'msg[receiver_type]':'1',
        'msg[msg_type]':'1',
        'msg[msg_status]':'0',
        'msg[content]':JSON.stringify({
          content:content
        }),
        'msg[timestamp]':Date.parse(new Date()),
        'msg[new_face_version]':'0',
        'msg[dev_id]':GUID,
        'from_firework':'0',
        'build':'0',
        'csrf_token':userData.bili_jct,
        'csrf':userData.bili_jct
      })
      console.log(postData)
      req.write(postData)
      req.end()
    }catch (e) {
      reject(e)
    }
  })
}

let GUID = guid()

function guid() {
  return 'xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx'.replace(/[xy]/g, function (c) {
    var r = Math.random() * 16 | 0,
      v = c == 'x' ? r : (r & 0x3 | 0x8);
    return v.toString(16);
  });
}

function getReceivedGifts(userData, gift_id, begin_time) {
  return new Promise((resolve, reject)=>{
    try {
      let options = {
        hostname: 'api.live.bilibili.com',
        path: '/xlive/revenue/v1/giftStream/getReceivedGiftStreamNextList?limit=20000&gift_id='+gift_id.toString()+'&begin_time='+begin_time,
        port: 443,
        method: 'GET',
        headers: {
          'cookie':'SESSDATA='+userData.SESSDATA
        }
      }
      let req = https.request(options, res => {
        let dd = ''
        res.on('data', chunk => {
          dd += chunk
        })
        res.on('end', () => {
          let resp = JSON.parse(dd.toString())
          if (resp.code === 0) {
            resolve(resp.data)
          } else {
            reject(resp)
          }
        })
        res.on('error', err => {
          reject(err)
        })
      })
      req.end()
    }catch (e) {
      reject(e)
    }
  })
}

function getReceivedGuards(userData, begin_time) {
  return new Promise((resolve, reject)=>{
    let guards = []
    let type_list = [10001, 10002, 10003]
    let promises = []
    for (let t of type_list) {
      promises.push(getReceivedGifts(userData, t, begin_time))
    }
    Promise.all(promises).then(res => {
      for (let r of res) {
        guards = guards.concat(r.list)
      }
      resolve(guards)
    }).catch(err => {
      reject(err)
    })
  })
}

export function getReceivedGuardsByPeriod(userData, begin_time, end_time) {
  return new Promise((resolve, reject)=>{
    let begin = new Date(begin_time)
    let end = new Date(end_time)
    let promises = []
    for (; begin <= end; begin.setDate(begin.getDate() + 1)) {
      promises.push(getReceivedGuards(userData, begin.toISOString().split('T')[0]))
    }
    Promise.all(promises).then(res => {
      let guards = []
      for (let r of res) {
        guards = guards.concat(r)
      }
      resolve(guards)
    }).catch(err => {
      reject(err)
    })
  })
}