import Vue from 'vue'
import Router from 'vue-router'

Vue.use(Router)

export default new Router({
  routes: [
    {
      path: '/',
      name: 'summary-page',
      component: require('@/components/SummaryPage').default
    },
    {
      path: '/guard_list',
      name: 'guard-list-page',
      component: require('@/components/GuardListPage').default
    },
    {
      path: '/setting',
      name: 'setting-page',
      component: require('@/components/SettingPage').default
    }
  ]
})
