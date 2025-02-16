import * as https from 'https'
import * as http from 'http'
import querystring from 'querystring'

function cookieString(userData) {
  if (userData && userData.SESSDATA) {
    return (
      'SESSDATA=' +
      encodeURIComponent(userData.SESSDATA) +
      '; DedeUserID=' +
      userData.DedeUserID +
      '; DedeUserID_ckMd5=' +
      userData.DedeUserID__ckMd5 +
      '; bili_jct=' +
      userData.bili_jct
    )
  }
  return ''
}

export function getUserInfoBySearch(userData, username) {
  return new Promise((resolve, reject) => {
    try {
      let options = {
        hostname: 'api.bilibili.com',
        path:
          '/x/web-interface/search/type?keyword=' +
          encodeURIComponent(username) +
          '&page=1&search_type=bili_user&order=totalrank&pagesize=5',
        port: 443,
        method: 'GET',
        headers: {
          cookie: cookieString(userData)
        }
      }
      let req = https.request(options, (res) => {
        let dd = ''
        res.on('data', (chunk) => {
          dd += chunk
        })
        res.on('end', () => {
          let resp = JSON.parse(dd.toString())
          if (resp.code === 0) {
            if (
              resp.data.result.length > 0 &&
              resp.data.result[0].uname == username
            ) {
              resolve(resp.data.result[0])
            } else {
              reject('no matched result')
            }
          } else {
            reject(resp)
          }
        })
        res.on('error', (err) => {
          reject(err)
        })
      })
      req.end()
    } catch (e) {
      reject(e)
    }
  })
}

export function getUserInfo(userData, mid) {
  return new Promise((resolve, reject) => {
    // https://line3-h5-mobile-api.biligame.com/game/center/h5/user/space/info?uid=475210&sdk_type=1
    let options = {
      hostname: 'line3-h5-mobile-api.biligame.com',
      path: `/game/center/h5/user/space/info?uid=${mid}&sdk_type=1`,
      port: 443,
      method: 'GET',
      headers: {
        cookie: cookieString(userData)
      }
    }
    let req = https.request(options, (res) => {
      let dd = ''
      res.on('data', (chunk) => {
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
      res.on('error', (err) => {
        reject(err)
      })
    })
    req.end()
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
          path: '/xlive/web-room/v1/index/getH5InfoByRoom?room_id=' + roomID
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
              reject(
                '/xlive/web-room/v1/index/getH5InfoByRoom?room_id=' + roomID
              )
            }
          })
        }
      )
    } catch (e) {
      reject(e)
    }
  })
}

export function getFollowerHistory(uid) {
  return new Promise((resolve, reject) => {
    try {
      https.get('https://api.vtbs.moe/v2/bulkActiveSome/' + uid, (res) => {
        let dd = ''
        res.on('data', (chunk) => {
          dd += chunk
        })
        res.on('end', () => {
          let resp = JSON.parse(dd.toString())
          resolve(resp)
        })
        res.on('error', (err) => {
          reject(err)
        })
      })
    } catch (e) {
      reject(e)
    }
  })
}

//https://api.vtbs.moe/v2/bulkGuard/61639371
export function getGuardHistory(uid) {
  return new Promise((resolve, reject) => {
    try {
      https.get('https://api.vtbs.moe/v2/bulkGuard/' + uid, (res) => {
        let dd = ''
        res.on('data', (chunk) => {
          dd += chunk
        })
        res.on('end', () => {
          let resp = JSON.parse(dd.toString())
          resolve(resp)
        })
        res.on('error', (err) => {
          reject(err)
        })
      })
    } catch (e) {
      reject(e)
    }
  })
}

export function getGuardList(uid, page, page_size) {
  return new Promise((resolve, reject) => {
    try {
      https.get(
        'https://api.live.bilibili.com/xlive/app-room/v2/guardTab/topList?roomid=1' +
          '&page=' +
          page +
          '&page_size=' +
          page_size +
          '&ruid=' +
          uid,
        (res) => {
          let dd = ''
          res.on('data', (chunk) => {
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
          res.on('error', (err) => {
            reject(err)
          })
        }
      )
    } catch (e) {
      reject(e)
    }
  })
}

export function getGuardValidDate(rid) {
  return new Promise((resolve, reject) => {
    try {
      http.get('http://guard.vjoi.cn/day?room=' + rid, (res) => {
        let dd = ''
        res.on('data', (chunk) => {
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
        res.on('error', (err) => {
          reject(err)
        })
      })
    } catch (e) {
      reject(e)
    }
  })
}

export function getGuardHistoryList(rid, date) {
  return new Promise((resolve, reject) => {
    try {
      http.get(
        'http://guard.vjoi.cn/history?room=' + rid + '&date=' + date,
        (res) => {
          let dd = ''
          res.on('data', (chunk) => {
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
          res.on('error', (err) => {
            reject(err)
          })
        }
      )
    } catch (e) {
      reject(e)
    }
  })
}

export function sendMessage(target, userData, content) {
  console.log(target, userData, content)
  let postData = querystring.stringify({
    'msg[sender_uid]': userData.DedeUserID,
    'msg[receiver_id]': target,
    'msg[receiver_type]': '1',
    'msg[msg_type]': '1',
    'msg[msg_status]': '0',
    'msg[content]': JSON.stringify({
      content: content
    }),
    'msg[timestamp]': (new Date().getTime() / 1000).toFixed(0),
    'msg[new_face_version]': '0',
    'msg[dev_id]': GUID,
    from_firework: '0',
    build: '0',
    csrf_token: userData.bili_jct,
    csrf: userData.bili_jct
  })
  return handleMessage(userData, postData)
}

export function recallMessage(target, userData, msg_key) {
  console.log(target, userData, msg_key)
  let postData = querystring.stringify({
    'msg[sender_uid]': userData.DedeUserID,
    'msg[receiver_id]': target,
    'msg[receiver_type]': '1',
    'msg[msg_type]': '5',
    'msg[msg_status]': '0',
    'msg[content]': msg_key,
    'msg[timestamp]': Date.parse(new Date()),
    'msg[new_face_version]': '0',
    'msg[dev_id]': GUID,
    from_firework: '0',
    build: '0',
    csrf_token: userData.bili_jct,
    csrf: userData.bili_jct
  })
  return handleMessage(userData, postData)
}

function handleMessage(userData, postData) {
  return new Promise((resolve, reject) => {
    try {
      let options = {
        hostname: 'api.vc.bilibili.com',
        path: '/web_im/v1/web_im/send_msg',
        port: 443,
        method: 'POST',
        headers: {
          'Content-Type': 'application/x-www-form-urlencoded',
          cookie: cookieString(userData)
        }
      }
      let req = https.request(options, (res) => {
        let dd = ''
        res.on('data', (chunk) => {
          dd += chunk
        })
        res.on('end', () => {
          // 将msg_key转字符串防止数字转换精度丢失
          dd = dd.replace(/"msg_key":(\d+)/, '"msg_key": "$1"')
          let resp = JSON.parse(dd.toString())
          if (resp.code === 0) {
            resolve(resp.data)
          } else {
            reject(resp)
          }
        })
        res.on('error', (err) => {
          reject(err)
        })
      })
      console.log(postData)
      req.write(postData)
      req.end()
    } catch (e) {
      reject(e)
    }
  })
}

let GUID = guid()

function guid() {
  return 'xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx'.replace(/[xy]/g, function (c) {
    var r = (Math.random() * 16) | 0,
      v = c == 'x' ? r : (r & 0x3) | 0x8
    return v.toString(16)
  })
}

function getReceivedGifts(userData, gift_id, begin_time) {
  return new Promise((resolve, reject) => {
    try {
      let options = {
        hostname: 'api.live.bilibili.com',
        path:
          '/xlive/revenue/v1/giftStream/getReceivedGiftStreamNextList?limit=20000&gift_id=' +
          gift_id.toString() +
          '&begin_time=' +
          begin_time,
        port: 443,
        method: 'GET',
        headers: {
          cookie: cookieString(userData)
        }
      }
      let req = https.request(options, (res) => {
        let dd = ''
        res.on('data', (chunk) => {
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
        res.on('error', (err) => {
          reject(err)
        })
      })
      req.end()
    } catch (e) {
      reject(e)
    }
  })
}

function getReceivedGuards(userData, begin_time) {
  return new Promise((resolve, reject) => {
    let guards = []
    let type_list = [10001, 10002, 10003]
    let promises = []
    for (let t of type_list) {
      promises.push(getReceivedGifts(userData, t, begin_time))
    }
    Promise.all(promises)
      .then((res) => {
        for (let r of res) {
          guards = guards.concat(r.list)
        }
        resolve(guards)
      })
      .catch((err) => {
        reject(err)
      })
  })
}

export function getReceivedGuardsByPeriod(userData, begin_time, end_time) {
  return new Promise((resolve, reject) => {
    let begin = new Date(begin_time)
    let end = new Date(end_time)
    let promises = []
    for (; begin <= end; begin.setDate(begin.getDate() + 1)) {
      promises.push(
        getReceivedGuards(userData, begin.toISOString().split('T')[0])
      )
    }
    Promise.all(promises)
      .then((res) => {
        let guards = []
        for (let r of res) {
          guards = guards.concat(r)
        }
        resolve(guards)
      })
      .catch((err) => {
        reject(err)
      })
  })
}

// https://api.bilibili.com/x/web-interface/nav
export function checkCookiesExpired(userData) {
  return new Promise((resolve, reject) => {
    try {
      let options = {
        hostname: 'api.bilibili.com',
        path: '/x/web-interface/nav',
        port: 443,
        method: 'GET',
        headers: {
          cookie: cookieString(userData)
        }
      }
      let req = https.request(options, (res) => {
        let dd = ''
        res.on('data', (chunk) => {
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
        res.on('error', (err) => {
          reject(err)
        })
      })
      req.end()
    } catch (e) {
      reject(e)
    }
  })
}
