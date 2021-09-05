import * as https from 'https'

export function getUserInfo(mid, callback) {
    https.get('https://api.bilibili.com/x/space/acc/info?mid=' + mid + '&jsonp=jsonp', res => {
        res.on('data', chunk => {
          console.log(chunk.toString())
          let resp = JSON.parse(chunk.toString())
          if (resp['code'] == 0) {
            callback(resp['data'])
          }
        })
      })
}