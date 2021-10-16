// src/plugins/vuetify.js
import 'vuetify/dist/vuetify.min.css'
import Vue from 'vue'
import Vuetify from 'vuetify'

Vue.use(Vuetify)

const opts = {
  theme: { dark: true }
}

export default new Vuetify(opts)
