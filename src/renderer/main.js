import Vue from 'vue'
import axios from 'axios'
import Store from 'electron-store'
import vuetify from './plugins/vuetify'
import * as bilibili from './bilibili/bilibili'

import App from './App'
import router from './router'

if (!process.env.IS_WEB) Vue.use(require('vue-electron'))

Vue.http = Vue.prototype.$http = axios
Vue.config.productionTip = false

Vue.prototype.Store = new Store();
Vue.prototype.Bilibili = bilibili;

/* eslint-disable no-new */
new Vue({
  components: { App },
  router,
  template: '<App/>',
  vuetify
}).$mount('#app')
