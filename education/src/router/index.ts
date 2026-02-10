import { createRouter, createWebHistory } from 'vue-router';
import { useUserStore } from '@/stores/user';

const routes = [
  {
    path: '/login',
    name: 'Login',
    component: () => import('@/views/Login/index.vue'),
    meta: { requiresAuth: false }
  },

];

const router = createRouter({
  history: createWebHistory(),
  routes
});

// 路由守卫
router.beforeEach((to, from, next) => {
  const userStore = useUserStore();
  
  if (to.meta.requiresAuth && !userStore.token) {
    // 需要登录但没有 token，跳转到登录页
    next({ name: 'Login', query: { redirect: to.fullPath } });
  } else if (to.name === 'Login' && userStore.token) {
    // 已登录状态下访问登录页，直接去首页
    next();
  } else {
    next();
  }
});

export default router;