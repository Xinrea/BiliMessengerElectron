import Vue from 'vue'
import Router from 'vue-router'

Vue.use(Router)

export default new Router({
  routes: [
    {
      path: '/',
      name: 'summary-page',
      component: require('@/components/SummaryPage').default,
      meta: {
        keepAlive: true
      }
    },
    {
      path: '/guard_list',
      name: 'guard-list-page',
      component: require('@/components/GuardListPage').default,
      meta: {
        keepAlive: true
      }
    },
    {
      path: '/template',
      name: 'template-page',
      component: require('@/components/TemplatePage').default,
      meta: {
        keepAlive: true
      }
    },
    {
      path: '/code',
      name: 'code-page',
      component: require('@/components/CodeManagePage').default,
      meta: {
        keepAlive: true
      }
    },
    {
      path: '/message',
      name: 'message-page',
      component: require('@/components/MessagePage').default,
      meta: {
        keepAlive: false
      }
    },
    {
      path: '/setting',
      name: 'setting-page',
      component: require('@/components/SettingPage').default,
      meta: {
        keepAlive: true
      }
    },
    {
      path: '/about',
      name: 'about-page',
      component: require('@/components/AboutPage').default,
      meta: {
        keepAlive: true
      }
    }
  ]
})
