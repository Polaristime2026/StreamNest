import axios from 'axios';
import { ElMessage, ElMessageBox } from 'element-plus';
import { useUserStore } from '@/stores/user';

const service = axios.create({
  baseURL: import.meta.env.VITE_APP_BASE_API,
  timeout: 10000
});

// 请求拦截：携带 Token
service.interceptors.request.use(config => {
  const userStore = useUserStore();
  if (userStore.token) {
    config.headers['Authorization'] = `Bearer ${userStore.token}`;
  }
  return config;
}, error => Promise.reject(error));

// 响应拦截：处理 Token 失效和全局错误
service.interceptors.response.use(
  response => {
    const res = response.data;
    // 根据后端约定的状态码判断
    if (res.code !== 200) {
      ElMessage.error(res.message || '系统开小差了');
      return Promise.reject(new Error(res.message || 'Error'));
    }
    return res;
  },
  error => {
    const userStore = useUserStore();
    const status = error.response?.status;

    switch (status) {
      case 401:
        ElMessageBox.confirm('登录已过期，请重新登录', '提示', {
          confirmButtonText: '确定',
          type: 'warning'
        }).then(() => {
          userStore.logout();
          location.reload(); // 强制刷新跳转登录页
        });
        break;
      case 403:
        ElMessage.error('没有权限访问该资源');
        break;
      case 500:
        ElMessage.error('服务器错误，请稍后再试');
        break;
      default:
        ElMessage.error('网络连接异常');
    }
    return Promise.reject(error);
  }
);

export default service;