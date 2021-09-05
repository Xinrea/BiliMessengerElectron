import * as https from 'https'

export function getUserInfo(mid, callback) {
    https.get('https://api.bilibili.com/x/space/acc/info?mid=' + mid + '&jsonp=jsonp', res => {
        let dd = ''
        res.on('data', chunk => {
          dd += chunk
        })
        res.on('end', ()=>{
          let resp = JSON.parse(dd.toString())
          if (resp['code'] == 0) {
            callback(resp['data'])
          }
        })
      })
}

export function getRoomInfo(roomID, resolve, reject) {
  // https://api.live.bilibili.com/xlive/web-room/v1/index/getH5InfoByRoom?room_id=
      https.get(
        'https://api.live.bilibili.com/xlive/web-room/v1/index/getH5InfoByRoom?room_id=' +
          roomID +
          '&jsonp=jsonp',
        (res) => {
          let dd = ''
          res.on('data', (chunk) => {
            dd += chunk
          })
          res.on('end', ()=>{
            let resp = JSON.parse(dd.toString())
            if (resp['code'] == 0) {
              resolve(resp['data'])
            } else {
              reject()
            }
          })
        }
      )
}