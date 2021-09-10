import * as https from 'https'

export function getUserInfo(mid, callback) {
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
            callback(resp['data'])
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

export function getGuardList(rid, uid, page, page_size) {
  return new Promise((resolve, reject)=>{
    try {
      https.get('https://api.live.bilibili.com/xlive/app-room/v2/guardTab/topList?roomid=' + rid+'&page='+page+'&page_size='+page_size+'&ruid='+uid, res => {
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