<template>
  <v-card
    height="100%"
  >
    <v-toolbar>
      兑换码总数：{{ codes.length }}
      <v-spacer />
      <v-dialog
        v-model="addDialog"
        max-width="600"
      >
        <template #activator="{ on, attrs }">
          <v-btn
            small
            color="green"
            v-bind="attrs"
            v-on="on"
          >
            <v-icon>
              mdi-plus-box
            </v-icon>
            导入
          </v-btn>
        </template>
        <v-card>
          <v-card-title>
            添加兑换码
          </v-card-title>
          <v-card-subtitle>
            一行一个，空行会被自动移除；爱发电可批量生成并复制粘贴于此
          </v-card-subtitle>
          <v-card-text>
            <v-textarea
              v-model="codeImport"
            />
            <v-btn
              color="primary"
              @click="importCodes"
            >
              添加
            </v-btn>
          </v-card-text>
        </v-card>
      </v-dialog>
      <v-btn
        small
        color="error"
        class="ml-1"
        @click="deleteCodes"
      >
        <v-icon>
          mdi-delete
        </v-icon>
        删除
        {{ codeSelected.length }}
      </v-btn>
    </v-toolbar>
    <v-list
      subheader
      style="bottom: 0; top: 60px; left: 0; right: 0; position: absolute"
    >
      <v-list-item-group
        :key="itemGroup"
        v-model="codeSelected"
        multiple
      >
        <v-list-item v-if="codes.length === 0">
          没有可用的兑换码
        </v-list-item>
        <v-list-item
          v-for="code in codes"
          :key="code"
        >
          <template #default="{ active }">
            <v-list-item-action>
              <v-checkbox :input-value="active" />
            </v-list-item-action>
            <v-list-item-content>
              {{ code }}
            </v-list-item-content>
          </template>
        </v-list-item>
      </v-list-item-group>
    </v-list>
  </v-card>
</template>

<script>
export default {
  name: 'CodeManagePage',
  data() {
    return {
      itemGroup: 0,
      codeImport: '',
      codes: ['code1', 'code2'],
      codeSelected: [],
      addDialog: false
    }
  },
  mounted () {
    this.loadCodes()
  },
  methods: {
    loadCodes() {
      this.codes = this.Store.get('codes', [])
      this.Store.onDidChange('codes', v=>{
        this.codes = v
      })
    },
    deleteCodes() {
      console.log('Delete code', this.codeSelected)
      for (let index = this.codes.length-1; index >= 0; index--) {
        if (this.codeSelected.includes(index)) {
          this.codes.splice(index,1)
        }
      }
      this.codeSelected = []
      this.Store.set('codes', this.codes)
      this.forceUpdate()
    },
    importCodes() {
      let newCodes = this.codeImport.split('\n').filter(c=>{
        return c !== ''
      })
      this.codes.push(...newCodes)
      this.addDialog = false
      this.Store.set('codes', this.codes)
      this.forceUpdate()
    },
    forceUpdate() {
      this.itemGroup += 1
    }
  }
}
</script>

<style scoped>

</style>