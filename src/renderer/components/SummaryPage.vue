<template>
  <div >
    <setting-alert-page v-if="!isSet"></setting-alert-page>
    <div v-else>summary</div>
  </div>
</template>

<script>
  import SettingAlertPage from './SettingAlertPage.vue'
  export default {
    name: 'summary-page',
    components: { SettingAlertPage },
    props: ['isSet', 'userdata'],
    methods: {
      open (link) {
        this.$electron.shell.openExternal(link)
      }
    }
  }
</script>

<style>

</style>
