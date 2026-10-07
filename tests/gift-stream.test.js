const assert = require('assert').strict
const { EventEmitter } = require('events')
const fs = require('fs')
const path = require('path')
const querystring = require('querystring')
const vm = require('vm')
const { transformSync } = require('@babel/core')

const source = fs.readFileSync(path.join(__dirname, '../src/renderer/bilibili/bilibili.js'), 'utf8')
const { code } = transformSync(source, {
  babelrc: false,
  configFile: false,
  plugins: ['@babel/plugin-transform-modules-commonjs']
})
const login = { SESSDATA: 'test-session', DedeUserID: '123', bili_jct: 'test-csrf', buvid3: 'test-device' }
const plain = value => JSON.parse(JSON.stringify(value))
const ready = (list = [], total_page = 0) => ({ code: 0, data: { ready: 1, list, total_page } })

function loadApi(reply) {
  const requests = []
  const delays = []
  let now = 0
  class TestDate extends Date {
    static now() { return now }
  }
  const https = {
    request(options, callback) {
      const request = new EventEmitter()
      let onTimeout
      request.setTimeout = (duration, handler) => { onTimeout = handler }
      request.destroy = error => process.nextTick(() => request.emit('error', error))
      request.end = body => {
        const record = { options, body, params: querystring.parse(body) }
        const index = requests.push(record) - 1
        process.nextTick(() => {
          const result = reply(record, index)
          if (result instanceof Error) return request.emit('error', result)
          if (result.timeout) return onTimeout()
          const response = new EventEmitter()
          callback(response)
          if (result.responseError) return response.emit('error', result.responseError)
          const text = typeof result === 'string' ? result : JSON.stringify(result)
          const split = Math.floor(text.length / 2)
          response.emit('data', Buffer.from(text.slice(0, split)))
          response.emit('data', Buffer.from(text.slice(split)))
          response.emit('end')
        })
      }
      return request
    }
  }
  const context = {
    exports: {}, Buffer, console, Date: TestDate,
    require: name => name === 'https' ? https : require(name),
    setTimeout(callback, delay) {
      delays.push(delay)
      now += delay
      process.nextTick(callback)
    }
  }
  vm.runInNewContext(code, context, { filename: 'bilibili.js' })
  return { fetch: context.exports.getReceivedGuardsByPeriod, requests, delays }
}

const tests = [
  ['posts the new guard filters and adapts all three guard levels', async () => {
    const gifts = [5, 6, 7].map(goods_id => ({ goods_id, uid: goods_id, uname: 'test', name: 'purchase', time: '2026-08-01 12:00:00' }))
    const api = loadApi(() => ready(gifts, 1))
    const guards = await api.fetch(login, '2026-08-01', '2026-08-31')
    assert.deepEqual(plain(guards), gifts.map((gift, index) => ({ ...gift, gift_name: ['总督', '提督', '舰长'][index] })))
    assert.equal(api.requests.length, 1)
    const { options, body, params } = api.requests[0]
    assert.equal(options.hostname, 'api.live.bilibili.com')
    assert.equal(options.path, '/xlive/revenue/v1/giftStream/getReceivedGiftStream')
    assert.equal(options.method, 'POST')
    assert.equal(options.headers['Content-Type'], 'application/x-www-form-urlencoded')
    assert.equal(options.headers['Content-Length'], Buffer.byteLength(body))
    assert.equal(options.headers.Origin, 'https://link.bilibili.com')
    assert.equal(options.headers.Referer, 'https://link.bilibili.com/p/center/index')
    assert.match(options.headers['User-Agent'], /Chrome\/154/)
    assert.match(options.headers.cookie, /SESSDATA=test-session/)
    assert.deepEqual(params, Object.assign(Object.create(null), {
      page: '0', gift_id: '0', begin_date: '20260801', end_date: '20260831', uname: '',
      goods_id: '5,6,7', csrf_token: 'test-csrf', csrf: 'test-csrf'
    }))
  }],
  ['fetches every zero-based page and preserves repeated purchases', async () => {
    const api = loadApi(({ params }) => ready([{ goods_id: 7, uid: 123, time: 'page-' + params.page }], params.page === '0' ? 3 : 0))
    const guards = await api.fetch(login, '2026-08-01', '2026-08-31')
    assert.deepEqual(api.requests.map(request => request.params.page), ['0', '1', '2'])
    assert.deepEqual(plain(guards.map(guard => guard.time)), ['page-0', 'page-1', 'page-2'])
    assert.equal(guards.length, 3)
  }],
  ['splits inclusive ranges across years and leap months', async () => {
    const api = loadApi(() => ready())
    assert.deepEqual(plain(await api.fetch(login, '2023-12-31', '2024-03-01')), [])
    assert.deepEqual(api.requests.map(({ params }) => [params.begin_date, params.end_date, params.page]), [
      ['20231231', '20231231', '0'], ['20240101', '20240131', '0'],
      ['20240201', '20240229', '0'], ['20240301', '20240301', '0']
    ])
  }],
  ['accepts a single-day period', async () => {
    const api = loadApi(() => ready())
    await api.fetch(login, '2026-10-06', '2026-10-06')
    assert.equal(api.requests.length, 1)
    assert.equal(api.requests[0].params.begin_date, '20261006')
    assert.equal(api.requests[0].params.end_date, '20261006')
  }],
  ['retries pending results without advancing the page', async () => {
    const api = loadApi((request, index) => index < 2 ? { code: 0, data: { ready: 0 } } : ready([{ goods_id: 7 }], 1))
    assert.equal((await api.fetch(login, '2026-08-01', '2026-08-31')).length, 1)
    assert.deepEqual(api.delays, [1000, 2000])
    assert.deepEqual(api.requests.map(({ params }) => params.page), ['0', '0', '0'])
  }],
  ['bounds polling when results never become ready', async () => {
    const api = loadApi(() => ({ code: 0, data: { ready: 0 } }))
    await assert.rejects(api.fetch(login, '2026-08-01', '2026-08-31'), /数据查询超时/)
    assert.equal(Math.max(...api.delays), 10000)
    assert.ok(api.delays.reduce((sum, delay) => sum + delay, 0) < 180000)
    assert.ok(api.requests.length < 25)
  }],
  ['uses CSRF from stored cookies', async () => {
    const api = loadApi(() => ready())
    await api.fetch({ cookies: 'SESSDATA=test-session; bili_jct=cookie-csrf; buvid3=test-device' }, '2026-08-01', '2026-08-31')
    assert.equal(api.requests[0].params.csrf, 'cookie-csrf')
    assert.equal(api.requests[0].params.csrf_token, 'cookie-csrf')
  }],
  ['rejects incomplete login and invalid periods before requesting', async () => {
    const api = loadApi(() => ready())
    await assert.rejects(api.fetch({}, '2026-08-01', '2026-08-31'), /登录信息不完整/)
    for (const [begin, end] of [[undefined, undefined], ['2026-08-01', undefined], ['2026-02-30', '2026-03-01'], ['20260801', '20260831'], ['2026-08-31', '2026-08-01']]) {
      await assert.rejects(api.fetch(login, begin, end))
    }
    assert.equal(api.requests.length, 0)
  }],
  ['preserves API login errors', async () => {
    const api = loadApi(() => ({ code: -101, message: '账号未登录' }))
    await assert.rejects(api.fetch(login, '2026-08-01', '2026-08-31'), error => error.code === -101)
  }],
  ['rejects HTML, malformed JSON, transport failures and timeouts', async () => {
    for (const [reply, message] of [
      ['<html>blocked</html>', /HTML/], ['{invalid', /JSON|Unexpected/],
      [new Error('network failure'), /network failure/],
      [{ responseError: new Error('response failure') }, /response failure/],
      [{ timeout: true }, /请求超时/]
    ]) {
      const api = loadApi(() => reply)
      await assert.rejects(api.fetch(login, '2026-08-01', '2026-08-31'), message)
    }
  }],
  ['rejects incomplete result metadata instead of returning partial records', async () => {
    for (const data of [null, {}, { ready: 1, list: null, total_page: 0 }, { ready: 1, list: [], total_page: -1 }, { ready: 1, list: [] }]) {
      const api = loadApi(() => ({ code: 0, data }))
      await assert.rejects(api.fetch(login, '2026-08-01', '2026-08-31'), /无效/)
    }
  }]
]

;(async () => {
  for (const [name, run] of tests) {
    await run()
    console.log('PASS ' + name)
  }
  console.log(tests.length + ' gift-stream tests passed')
})().catch(error => {
  console.error(error)
  process.exitCode = 1
})
