import * as https from 'https'
import * as http from 'http'
import querystring from 'querystring'
import * as crypto from 'crypto'

let wbiKeysCache = null
let wbiKeysCacheTime = 0
const WBI_KEYS_CACHE_DURATION = 5 * 60 * 1000 // 5分钟缓存
let biliTicketCache = null
const MESSAGE_BUVID3_KEY_PREFIX = 'im_buvid3_'
const MESSAGE_DEV_ID_KEY_PREFIX = 'im_deviceid_'

function parseCookieString(cookie) {
  let parsed = {}
  if (!cookie) {
    return parsed
  }
  cookie.split(';').forEach(part => {
    let item = part.trim()
    if (!item) {
      return
    }
    let sep = item.indexOf('=')
    if (sep === -1) {
      return
    }
    parsed[item.slice(0, sep)] = item.slice(sep + 1)
  })
  return parsed
}

function buildCookieString(cookies) {
  return Object.keys(cookies)
    .filter(key => cookies[key] !== undefined && cookies[key] !== null && cookies[key] !== '')
    .map(key => key + '=' + cookies[key])
    .join('; ')
}

function normalizeSessdata(value) {
  if (!value) {
    return value
  }
  let normalized = String(value).trim()
  for (let i = 0; i < 2; i++) {
    try {
      let decoded = decodeURIComponent(normalized)
      if (decoded === normalized) {
        break
      }
      normalized = decoded
    } catch (e) {
      break
    }
  }
  return normalized
}

function readLocalStorage(key) {
  try {
    if (typeof window !== 'undefined' && window.localStorage) {
      return window.localStorage.getItem(key)
    }
  } catch (e) {
    console.warn('读取localStorage失败:', e)
  }
  return ''
}

function writeLocalStorage(key, value) {
  try {
    if (typeof window !== 'undefined' && window.localStorage) {
      window.localStorage.setItem(key, value)
    }
  } catch (e) {
    console.warn('写入localStorage失败:', e)
  }
}

function getMessageDevId(userData) {
  let uid = userData && userData.DedeUserID ? String(userData.DedeUserID) : ''
  if (!uid) {
    return guid(true)
  }
  let storageKey = MESSAGE_DEV_ID_KEY_PREFIX + uid
  let cached = readLocalStorage(storageKey)
  if (cached) {
    return cached
  }
  let devId = guid(true)
  writeLocalStorage(storageKey, devId)
  return devId
}

function getBuvid3(userData) {
  if (userData && userData.buvid3) {
    return userData.buvid3
  }
  if (userData && userData.cookies) {
    let cookieMap = parseCookieString(userData.cookies)
    if (cookieMap.buvid3) {
      return cookieMap.buvid3
    }
  }
  let uid = userData && userData.DedeUserID ? String(userData.DedeUserID) : 'default'
  let storageKey = MESSAGE_BUVID3_KEY_PREFIX + uid
  let cached = readLocalStorage(storageKey)
  if (cached) {
    return cached
  }
  // send_msg 缺少 buvid3 时会稳定返回 412，这里补一个持久化设备标识即可绕过风控校验
  let buvid3 = guid(true)
  writeLocalStorage(storageKey, buvid3)
  return buvid3
}

function cookieString(userData) {
  if (!userData) return ''
  let cookies = userData.cookies ? parseCookieString(userData.cookies) : {}
  if (userData.SESSDATA) {
    // 兼容旧版本持久化后的编码值，避免 SESSDATA 仍是 %252C... 这类双编码导致 -101
    cookies.SESSDATA = normalizeSessdata(userData.SESSDATA)
  }
  if (userData.DedeUserID !== undefined && userData.DedeUserID !== null) {
    cookies.DedeUserID = userData.DedeUserID
  }
  if (userData.DedeUserID__ckMd5) {
    cookies.DedeUserID__ckMd5 = userData.DedeUserID__ckMd5
  }
  if (userData.bili_jct) {
    cookies.bili_jct = userData.bili_jct
  }

  ['buvid4', '_uuid', 'b_nut', 'b_lsid', 'sid'].forEach(key => {
    if (userData[key] && !cookies[key]) {
      cookies[key] = userData[key]
    }
  })

  let ticket = userData.bili_ticket || biliTicketCache
  if (ticket && !cookies.bili_ticket) {
    cookies.bili_ticket = ticket
  }
  if (!cookies.buvid3) {
    cookies.buvid3 = getBuvid3(userData)
  }
  return buildCookieString(cookies)
}

// 获取wbi签名密钥
function getWbiKeys(userData) {
  return new Promise((resolve, reject) => {
    // 检查缓存
    if (wbiKeysCache && Date.now() - wbiKeysCacheTime < WBI_KEYS_CACHE_DURATION) {
      resolve(wbiKeysCache)
      return
    }

    let options = {
      hostname: 'api.bilibili.com',
      path: '/x/web-interface/nav',
      port: 443,
      method: 'GET',
      headers: {
        cookie: cookieString(userData),
        'User-Agent': 'Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/147.0.0.0 Safari/537.36'
      }
    }
    let req = https.request(options, (res) => {
      // 从Set-Cookie中提取bili_ticket
      let setCookieHeaders = res.headers['set-cookie']
      if (setCookieHeaders) {
        for (let cookie of setCookieHeaders) {
          let match = cookie.match(/bili_ticket=([^;]+)/)
          if (match) {
            biliTicketCache = match[1]
            console.log('从nav接口捕获到bili_ticket:', biliTicketCache.substring(0, 50) + '...')
          }
        }
      }
      if (!biliTicketCache) {
        console.warn('未从nav接口捕获到bili_ticket，请手动设置userData.bili_ticket或userData.cookies')
      }
      let dd = ''
      res.on('data', (chunk) => {
        dd += chunk
      })
      res.on('end', () => {
        try {
          let resp = JSON.parse(dd.toString())
          if (resp.data && resp.data.wbi_img) {
            let imgUrl = resp.data.wbi_img.img_url
            let subUrl = resp.data.wbi_img.sub_url
            let imgKey = imgUrl.match(/wbi\/([^.]+)\.png/)[1]
            let subKey = subUrl.match(/wbi\/([^.]+)\.png/)[1]
            wbiKeysCache = imgKey + subKey
            wbiKeysCacheTime = Date.now()
            resolve(wbiKeysCache)
          } else {
            reject(resp)
          }
        } catch (e) {
          reject(e)
        }
      })
      res.on('error', (err) => {
        reject(err)
      })
    })
    req.end()
  })
}

// 生成wbi签名
function getWbiSign(params, wbiKey, wts) {
  let table = [
    46, 47, 18, 2, 53, 8, 23, 32, 15, 50, 10, 31, 58, 3, 45, 35, 27, 43, 5, 49, 33, 9, 42,
    19, 29, 28, 14, 39, 12, 38, 41, 13, 37, 48, 7, 16, 24, 55, 40, 61, 26, 17, 0, 1, 60,
    51, 30, 4, 22, 25, 54, 21, 56, 59, 6, 63, 57, 62, 11, 36, 20, 34, 44, 52
  ]
  let encoded = []
  table.forEach(x => {
    if (x < wbiKey.length) {
      encoded.push(wbiKey.charCodeAt(x))
    }
  })
  encoded = encoded.slice(0, 32)
  let encodedStr = String.fromCharCode(...encoded)

  if (wts === undefined) {
    wts = Math.floor(Date.now() / 1000)
  }
  params.wts = wts

  let keys = Object.keys(params).sort()
  let query = []
  keys.forEach(key => {
    let value = params[key]
    if (typeof value === 'object') {
      value = JSON.stringify(value)
    }
    value = value.toString().replace(/[!'()*]/g, '')
    value = encodeURIComponent(value)
    query.push(key + '=' + value)
  })

  let queryStr = query.join('&')
  let md5Hash = crypto.createHash('md5')
  md5Hash.update(queryStr + encodedStr)
  let wRid = md5Hash.digest('hex')

  return queryStr + '&w_rid=' + wRid
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
  return getWbiKeys(userData)
    .then(wbiKey => {
      let wts = Math.floor(Date.now() / 1000)
      let devId = getMessageDevId(userData)
      // WBI签名参数使用 w_ 前缀，放在URL query string
      let wbiParams = {
        w_sender_uid: userData.DedeUserID,
        w_receiver_id: target,
        w_dev_id: devId
      }
      let signedQuery = getWbiSign(wbiParams, wbiKey, wts)

      // POST body参数使用 msg[ 前缀
      let postDataObj = {
        'msg[sender_uid]': userData.DedeUserID,
        'msg[receiver_type]': '1',
        'msg[receiver_id]': target,
        'msg[msg_type]': '1',
        'msg[msg_status]': '0',
        'msg[content]': JSON.stringify({ content: content }),
        'msg[new_face_version]': '0',
        'msg[canal_token]': '',
        'msg[dev_id]': devId,
        'msg[timestamp]': wts,
        from_firework: '0',
        build: '0',
        mobi_app: 'web',
        csrf: userData.bili_jct
      }
      let postData = querystring.stringify(postDataObj)
      return handleMessage(userData, postData, signedQuery)
    })
    .catch(err => {
      console.error('sendMessage失败:', err)
      throw err
    })
}

export function recallMessage(target, userData, msg_key) {
  console.log(target, userData, msg_key)
  let devId = getMessageDevId(userData)
  let postData = querystring.stringify({
    'msg[sender_uid]': userData.DedeUserID,
    'msg[receiver_id]': target,
    'msg[receiver_type]': '1',
    'msg[msg_type]': '5',
    'msg[msg_status]': '0',
    'msg[content]': msg_key,
    'msg[timestamp]': Date.parse(new Date()),
    'msg[new_face_version]': '0',
    'msg[dev_id]': devId,
    from_firework: '0',
    build: '0',
    csrf_token: userData.bili_jct,
    csrf: userData.bili_jct
  })
  return handleMessage(userData, postData)
}

function handleMessage(userData, postData, signedQuery) {
  return new Promise((resolve, reject) => {
    try {
      let path = '/web_im/v1/web_im/send_msg'
      if (signedQuery) {
        path = path + '?' + signedQuery
      }
      let options = {
        hostname: 'api.vc.bilibili.com',
        path: path,
        port: 443,
        method: 'POST',
        headers: {
          'Content-Type': 'application/x-www-form-urlencoded',
          cookie: cookieString(userData),
          Accept: '*/*',
          'Accept-Language': 'zh-CN,zh;q=0.9,en;q=0.8',
          'Cache-Control': 'no-cache',
          Pragma: 'no-cache',
          Priority: 'u=1, i',
          Origin: 'https://message.bilibili.com',
          Referer: 'https://message.bilibili.com/',
          'sec-ch-ua': '"Google Chrome";v="147", "Not.A/Brand";v="8", "Chromium";v="147"',
          'sec-ch-ua-mobile': '?0',
          'sec-ch-ua-platform': '"macOS"',
          'sec-fetch-dest': 'empty',
          'sec-fetch-mode': 'cors',
          'sec-fetch-site': 'same-site',
          'User-Agent': 'Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/147.0.0.0 Safari/537.36'
        }
      }
      console.log('开始发送请求')
      let req = https.request(options, (res) => {
        let dd = ''
        res.on('data', (chunk) => {
          dd += chunk
        })
        res.on('end', () => {
          // 将msg_key转字符串防止数字转换精度丢失
          dd = dd.replace(/"msg_key":(\d+)/, '"msg_key": "$1"')
          // 检查是否为HTML响应（API错误或需要重新登录）
          if (dd.toString().trim().startsWith('<')) {
            console.error('API返回HTML响应:', dd.toString())
            reject(new Error('API返回了HTML响应，可能是登录失效或请求被拒绝'))
            return
          }
          console.log('API响应原始内容:', dd.toString())
          let resp = JSON.parse(dd.toString())
          if (resp.code === 0) {
            resolve(resp.data)
          } else {
            console.error('API错误响应:', resp)
            reject(new Error(JSON.stringify(resp)))
          }
        })
        res.on('error', (err) => {
          reject(err)
        })
      })
      console.log('send_msg已组装完成')
      if (postData) {
        req.write(postData)
      }
      req.end()
    } catch (e) {
      console.error('sendMessage error:', e)
      reject(e)
    }
  })
}

function guid(upperCase = false) {
  let value = 'xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx'.replace(/[xy]/g, function (c) {
    var r = (Math.random() * 16) | 0,
      v = c == 'x' ? r : (r & 0x3) | 0x8
    return v.toString(16)
  })
  return upperCase ? value.toUpperCase() : value
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
