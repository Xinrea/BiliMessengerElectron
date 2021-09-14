import * as https from 'https'
import * as http from 'http'
import querystring from 'querystring'

export function getUserInfo(mid) {
  return new Promise((resolve, reject) => {
    try {
      https.get('https://api.bilibili.com/x/space/acc/info?mid=' + mid + '&jsonp=jsonp', res => {
        let dd = ''
        res.on('data', chunk => {
          dd += chunk
        })
        res.on('end', () => {
          let resp = JSON.parse(dd.toString())
          if (resp['code'] === 0) {
            resolve(resp['data'])
          } else {
            reject(resp)
          }
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
      http.get('http://guard.joi-club.cn/day?room='+rid, res => {
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
      http.get('http://guard.joi-club.cn/history?room='+rid+'&date='+date, res => {
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
        port: 80,
        method: 'POST',
        headers: {
          'Content-Type':'application/x-www-form-urlencoded',
          'cookie':'SESSDATA='+userData.SESSDATA
        }
      }
      let req = http.request(options, res => {
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