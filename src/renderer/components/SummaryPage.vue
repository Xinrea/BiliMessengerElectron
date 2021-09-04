<template>
  <div>
    <setting-alert-page v-if="!isSet" />
    <div v-else>
      summary
    </div>
  </div>
</template>

<script>
  import SettingAlertPage from './SettingAlertPage.vue'
  export default {
    name: 'SummaryPage',
    components: { SettingAlertPage },
    props: {
      isSet:{
        type: Boolean,
        default: false
      },
      userdata:{
        type: Object,
        default: null
      }
      },
    methods: {
      open (link) {
        this.$electron.shell.openExternal(link)
      }
    }
  }
</script>

<style>

</style>
