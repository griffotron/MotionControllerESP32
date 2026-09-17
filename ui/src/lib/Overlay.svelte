<script lang="ts">
    let { active, children } = $props();
</script>

<div class="relative">
    {#if active}
        <div class="absolute inset-0 z-50 flex items-center justify-center bg-base-300/60 backdrop-blur-sm rounded-inherit">
            <span class="loading loading-spinner loading-lg"></span>
        </div>
    {/if}
    {@render children()}
</div>