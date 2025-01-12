// @ts-check
// `@type` JSDoc annotations allow editor autocompletion and type checking
// (when paired with `@ts-check`).
// There are various equivalent ways to declare your Docusaurus config.
// See: https://docusaurus.io/docs/api/docusaurus-config

import { themes as prismThemes } from 'prism-react-renderer';

// This runs in Node.js - Don't use client-side code here (browser APIs, JSX...)

/** @type {import('@docusaurus/types').Config} */
const config = {
  title: 'SLJIT',
  tagline: 'Platform independent low-level JIT compiler',

  // Set the production url of your site here
  url: 'https://zherczeg.github.com',
  // Set the /<baseUrl>/ pathname under which your site is served
  // For GitHub pages deployment, it is often '/<projectName>/'
  baseUrl: '/sljit/',

  // GitHub pages deployment config
  organizationName: 'zherczeg', // GitHub org/user
  projectName: 'sljit', // Repo

  onBrokenLinks: 'throw',
  onBrokenMarkdownLinks: 'warn',

  i18n: {
    defaultLocale: 'en',
    locales: ['en'],
  },

  presets: [
    [
      'classic',
      /** @type {import('@docusaurus/preset-classic').Options} */
      ({
        docs: {
          path: '..', // '.' would be the website folder itself
          include: [
            'api/**/*.{md,mdx}',
            'general/**/*.{md,mdx}',
            'tutorial/**/*.{md,mdx}'
          ],
          sidebarPath: './sidebars.js',
          // Please change this to your repo.
          // Remove this to remove the "edit this page" links.
          editUrl:
            'https://github.com/zherczeg/sljit/docs/docs/',
        },
        blog: false,
        theme: {
          customCss: './src/css/custom.css',
        },
      }),
    ],
  ],

  themeConfig:
    /** @type {import('@docusaurus/preset-classic').ThemeConfig} */
    ({
      navbar: {
        title: 'SLJIT',
        items: [
          {
            type: 'docSidebar',
            sidebarId: 'generalSidebar',
            position: 'left',
            label: 'Docs',
          },
          {
            type: 'docSidebar',
            sidebarId: 'tutorialSidebar',
            position: 'left',
            label: 'Tutorial',
          },
          {
            type: 'docSidebar',
            sidebarId: 'apiSidebar',
            position: 'left',
            label: 'API',
          },
          {
            'aria-label': 'GitHub',
            className: 'navbar--github-link',
            href: 'https://github.com/zherczeg/sljit',
            position: 'right',
          },
        ],
      },
      docs: {
        sidebar: {
          hideable: true,
        },
      },
      footer: {
        style: 'light',
        links: [
          {
            title: 'Content',
            items: [
              {
                label: 'Docs',
                to: '/docs/general/introduction',
              },
              {
                label: 'Tutorial',
                to: '/docs/tutorial/overview',
              },
            ],
          },
          {
            title: 'More',
            items: [
              {
                label: 'GitHub',
                href: 'https://github.com/zherczeg/sljit',
              },
            ],
          },
          {
            title: 'Attribution',
            items: [
              {
                html: `
                  <span>
                    <a href="https://www.figma.com/community/file/1166831539721848736/solar-icons-set">
                      Solar Icon Set
                    </a>
                    by
                    <a href="https://www.figma.com/@480design">
                      480 Design
                    </a>
                    and
                    <a href="https://www.figma.com/@voidrainbow">
                      R4IN80W
                    </a>
                    |
                    <a href="http://creativecommons.org/licenses/by/4.0/">
                      CC BY 4.0
                    </a>
                  </span>
                `
              }
            ]
          }
        ],
        copyright: `Copyright © ${new Date().getFullYear()} SLJIT contributors. Built with Docusaurus.`,
      },
      prism: {
        theme: prismThemes.github,
        darkTheme: prismThemes.dracula,
      },
    }),
};

export default config;
